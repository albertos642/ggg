"""
@file apply_kconfig.py
@brief Automated Kconfig parser, C++ header generator, and PlatformIO build integrator.

@author Alberto Soncini <alberto@synergon-lab.xyz>
@copyright Copyright (c) 2012-2026 Alberto Soncini.
@license GPL-3.0-or-later

This file is part of GGG.

GGG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

GGG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GGG. If not, see <https://www.gnu.org/licenses/>.
"""

#!/usr/bin/env python3
"""
GGG (General Gadget Generator) Kconfig Manager & PlatformIO Integration
Agnostic pre-build script that:
1. Loads Kconfig configuration (.config).
2. Generates C++ macro header (include/autoconf.h).
3. Recursively discovers active plugins and modules in both GGG root and consumer project root.
4. Injects include directories (CPPPATH) and external library dependencies (LIB_DEPS) into PlatformIO.
"""

import os
import sys
import re
import subprocess

# Auto-install kconfiglib if missing
try:
    import kconfiglib
except ImportError:
    print("\n[GGG Kconfig] kconfiglib not found in Python environment. Auto-installing...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "kconfiglib"])
        import kconfiglib
        print("[GGG Kconfig] kconfiglib successfully installed!\n")
    except Exception as e:
        print(f"[GGG Kconfig] ERROR: Failed to install kconfiglib: {e}")
        sys.exit(1)

# PlatformIO / SCons Environment Detection
try:
    Import("env")
    IS_PIO_ENV = True
except (NameError, ImportError):
    env = None
    IS_PIO_ENV = False

def is_subpath(path, parent):
    """Returns True if path is equal to or inside parent directory."""
    try:
        rel = os.path.relpath(path, parent)
        return not rel.startswith("..")
    except ValueError:
        return False

def get_script_dir():
    """Robustly retrieves the script's directory across standalone and SCons environments."""
    if "__file__" in globals() and __file__:
        return os.path.dirname(os.path.abspath(__file__))
    try:
        frame_file = sys._getframe(1).f_code.co_filename
        if frame_file and (os.path.exists(frame_file) or os.path.isabs(frame_file)):
            return os.path.dirname(os.path.abspath(frame_file))
    except Exception:
        pass
    import inspect
    frame = inspect.currentframe()
    while frame:
        fname = frame.f_code.co_filename
        if fname.endswith("apply_kconfig.py") and os.path.exists(fname):
            return os.path.dirname(os.path.abspath(fname))
        frame = frame.f_back
    return os.path.abspath(os.getcwd())

def resolve_directories():
    """Resolves GGG root directory, project directory and execution mode."""
    script_dir = get_script_dir()
    ggg_root = os.path.abspath(os.path.join(script_dir, ".."))
    
    if IS_PIO_ENV and env.get("PROJECT_DIR"):
        project_dir = os.path.abspath(env.get("PROJECT_DIR"))
    else:
        project_dir = os.path.abspath(os.getcwd())
        
    is_standalone = (os.path.normcase(project_dir) == os.path.normcase(ggg_root))
    return ggg_root, project_dir, is_standalone

def locate_kconfig_files(ggg_root, project_dir):
    """Finds root Kconfig and .config files with appropriate priority."""
    proj_kconfig = os.path.join(project_dir, "Kconfig")
    ggg_kconfig = os.path.join(ggg_root, "Kconfig")
    
    if os.path.isfile(proj_kconfig):
        kconfig_file = proj_kconfig
    elif os.path.isfile(ggg_kconfig):
        kconfig_file = ggg_kconfig
    else:
        kconfig_file = None

    proj_dot_config = os.path.join(project_dir, ".config")
    ggg_dot_config = os.path.join(ggg_root, ".config")
    
    if os.path.isfile(proj_dot_config):
        dot_config_file = proj_dot_config
    elif os.path.isfile(ggg_dot_config):
        dot_config_file = ggg_dot_config
    else:
        dot_config_file = None

    return kconfig_file, dot_config_file

def index_kconfig_symbols(kconf, srctree):
    """Maps normalized filepaths of sub-Kconfig files to their defined symbols."""
    file_to_syms = {}
    for name, sym in kconf.syms.items():
        for node in sym.nodes:
            if node.filename:
                abs_path = os.path.normcase(os.path.abspath(os.path.join(srctree, node.filename)))
                file_to_syms.setdefault(abs_path, []).append((sym, node))
    return file_to_syms

def is_module_active(kconfig_path, file_to_syms, kconf):
    """
    Determines if a module defined by a Kconfig file is enabled in the current configuration.
    Prefers AST examination via kconfiglib nodes, falls back to text parsing if unindexed.
    """
    norm_path = os.path.normcase(os.path.abspath(kconfig_path))
    
    if norm_path in file_to_syms:
        sym_nodes = file_to_syms[norm_path]
        # 1. Check if there is an explicit menuconfig symbol
        for sym, node in sym_nodes:
            if getattr(node, "is_menuconfig", False):
                return sym.str_value in ('y', 'm'), sym.name
        # 2. Check the first bool/tristate symbol
        for sym, node in sym_nodes:
            if sym.type in (kconfiglib.BOOL, kconfiglib.TRISTATE):
                return sym.str_value in ('y', 'm'), sym.name
        # 3. Check if ANY symbol in this file is enabled
        for sym, node in sym_nodes:
            if sym.str_value in ('y', 'm'):
                return True, sym.name
        return False, None

    # Fallback text parsing if not indexed in kconf AST
    try:
        with open(kconfig_path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                match = re.match(r"^\s*(?:menu)?config\s+([A-Za-z0-9_]+)", line)
                if match:
                    sname = match.group(1)
                    if sname in kconf.syms and kconf.syms[sname].str_value in ('y', 'm'):
                        return True, sname
    except Exception:
        pass

    return False, None

def scan_modules(search_roots, kconfig_file, file_to_syms, kconf):
    """
    Recursively scans candidate directories for Kconfig files, detecting active modules,
    include paths, and external dependencies.
    """
    active_modules = []
    cpp_paths = []
    extra_lib_dirs = set()
    libs_to_inject = []
    
    ignored_dirs = {
        ".git", ".pio", ".venv", "__pycache__", ".vscode",
        "node_modules", "scripts", "test", "build"
    }

    norm_root_kconfig = os.path.normcase(os.path.abspath(kconfig_file))
    visited_kconfigs = set()

    for base_root in search_roots:
        if not os.path.exists(base_root):
            continue
            
        for root, dirs, files in os.walk(base_root):
            # Prune ignored directories
            dirs[:] = [d for d in dirs if d not in ignored_dirs and not d.startswith(".")]

            if "Kconfig" in files:
                current_kconfig = os.path.join(root, "Kconfig")
                norm_current = os.path.normcase(os.path.abspath(current_kconfig))

                # Skip root Kconfig
                if norm_current == norm_root_kconfig or norm_current in visited_kconfigs:
                    continue
                visited_kconfigs.add(norm_current)

                active, primary_sym = is_module_active(current_kconfig, file_to_syms, kconf)
                if active:
                    active_modules.append((root, primary_sym))
                    
                    # Module directory and optional include/ subdirectory
                    cpp_paths.append(root)
                    inc_sub = os.path.join(root, "include")
                    if os.path.isdir(inc_sub):
                        cpp_paths.append(inc_sub)

                    # If this module is directly under a 'plugins' folder, add parent to extra_lib_dirs
                    parent_dir = os.path.dirname(root)
                    if os.path.basename(parent_dir).lower() == "plugins":
                        extra_lib_dirs.add(parent_dir)

                    # Check for dependencies.txt
                    dep_file = os.path.join(root, "dependencies.txt")
                    if os.path.isfile(dep_file):
                        with open(dep_file, "r", encoding="utf-8") as df:
                            for line in df:
                                line = line.strip()
                                if line and not line.startswith("#"):
                                    if line not in libs_to_inject:
                                        libs_to_inject.append(line)

    return active_modules, cpp_paths, list(extra_lib_dirs), libs_to_inject

def main():
    if IS_PIO_ENV:
        if env.get("__GGG_KCONFIG_APPLIED__"):
            return
        env["__GGG_KCONFIG_APPLIED__"] = True

    print("==================================================")
    print("      GGG Modular Kconfig & Build Engine          ")
    print("==================================================")
    
    ggg_root, project_dir, is_standalone = resolve_directories()
    mode_str = "Standalone Framework" if is_standalone else "Library / Consumer Application"
    print(f"[*] Mode:         {mode_str}")
    print(f"[*] GGG Root:     {ggg_root}")
    print(f"[*] Project Root: {project_dir}")

    kconfig_file, dot_config_file = locate_kconfig_files(ggg_root, project_dir)
    if not kconfig_file:
        print("[!] ERROR: No root Kconfig found in project or GGG directory!")
        return

    # Set srctree environment variable for kconfiglib
    srctree = os.path.dirname(kconfig_file)
    os.environ["srctree"] = srctree
    print(f"[*] Root Kconfig: {kconfig_file}")

    kconf = kconfiglib.Kconfig(kconfig_file)
    if dot_config_file:
        print(f"[*] Loaded Config: {dot_config_file}")
        kconf.load_config(dot_config_file)
    else:
        print("[*] No .config file found; applying default values.")

    # Generate autoconf.h
    target_headers = [os.path.join(project_dir, "include", "autoconf.h")]
    if not is_standalone:
        target_headers.append(os.path.join(ggg_root, "include", "autoconf.h"))

    for hdr in target_headers:
        os.makedirs(os.path.dirname(hdr), exist_ok=True)
        kconf.write_autoconf(hdr)
        print(f"[+] autoconf.h generated: {os.path.relpath(hdr, project_dir)}")

    # Index symbol definitions
    file_to_syms = index_kconfig_symbols(kconf, srctree)

    # Establish search roots for modules/plugins
    search_roots = [
        os.path.join(ggg_root, "plugins"),
        os.path.join(ggg_root, "lib")
    ]
    if not is_standalone:
        search_roots.extend([
            os.path.join(project_dir, "plugins"),
            os.path.join(project_dir, "lib")
        ])

    active_modules, cpp_paths, extra_lib_dirs, libs_to_inject = scan_modules(
        search_roots, kconfig_file, file_to_syms, kconf
    )

    if active_modules:
        print(f"[*] Active Modules Discovered ({len(active_modules)}):")
        for mod_dir, sym in active_modules:
            rel = os.path.relpath(mod_dir, project_dir)
            print(f"    - [{sym or 'ACTIVE'}]: {rel}")
    else:
        print("[*] No dynamic modules or plugins currently active.")

    # Base include paths
    base_includes = [
        os.path.join(ggg_root, "include"),
        os.path.join(project_dir, "include")
    ]
    all_cpp_paths = []
    for p in base_includes + cpp_paths:
        if os.path.exists(p) and p not in all_cpp_paths:
            all_cpp_paths.append(p)

    # Collect include paths from installed project libdeps (e.g. FreeRTOS, etc.)
    if IS_PIO_ENV:
        pioenv = env.get("PIOENV", "")
        if pioenv:
            libdeps_env_dir = os.path.join(project_dir, ".pio", "libdeps", pioenv)
            if os.path.isdir(libdeps_env_dir):
                for lib_name in os.listdir(libdeps_env_dir):
                    lib_path = os.path.join(libdeps_env_dir, lib_name)
                    if os.path.isdir(lib_path):
                        for sub in ["src", "include", ""]:
                            cand = os.path.normpath(os.path.join(lib_path, sub)) if sub else lib_path
                            if os.path.isdir(cand) and cand not in all_cpp_paths:
                                all_cpp_paths.append(cand)

            try:
                platform = env.PioPlatform()
                for pkg_name in platform.packages.keys():
                    pkg_dir = platform.get_package_dir(pkg_name)
                    if pkg_dir:
                        libs_dir = os.path.join(pkg_dir, "libraries")
                        if os.path.isdir(libs_dir):
                            for lib_entry in os.listdir(libs_dir):
                                cand = os.path.join(libs_dir, lib_entry)
                                if os.path.isdir(cand) and cand not in all_cpp_paths:
                                    all_cpp_paths.append(cand)
                                cand_src = os.path.join(cand, "src")
                                if os.path.isdir(cand_src) and cand_src not in all_cpp_paths:
                                    all_cpp_paths.append(cand_src)
            except Exception:
                pass

    # In native environment, ensure all libraries and plugins in search_roots are in CPPPATH
    if IS_PIO_ENV and env.get("PIOPLATFORM") == "native":
        for s_root in search_roots:
            if os.path.isdir(s_root):
                for item in os.listdir(s_root):
                    item_path = os.path.join(s_root, item)
                    if os.path.isdir(item_path):
                        inc_cand = os.path.join(item_path, "include")
                        if os.path.isdir(inc_cand) and inc_cand not in all_cpp_paths:
                            all_cpp_paths.append(inc_cand)
                        if item_path not in all_cpp_paths:
                            all_cpp_paths.append(item_path)

    # Inject into PlatformIO / SCons build environment
    if IS_PIO_ENV:
        if all_cpp_paths:
            env.Append(CPPPATH=all_cpp_paths)
            print(f"[+] Injected CPPPATH ({len(all_cpp_paths)} paths)")

        if libs_to_inject:
            print(f"[+] Injected LIB_DEPS ({len(libs_to_inject)} libraries): {libs_to_inject}")
            env.Append(LIB_DEPS=libs_to_inject)

        if extra_lib_dirs:
            env.Append(LIB_EXTRA_DIRS=extra_lib_dirs)
            print(f"[+] Injected LIB_EXTRA_DIRS: {extra_lib_dirs}")

        # Compile sources for active modules (plugins, HAL drivers, core modules)
        for mod_dir, sym in active_modules:
            src_dir = os.path.join(mod_dir, "src") if os.path.isdir(os.path.join(mod_dir, "src")) else mod_dir
            if os.path.isdir(src_dir):
                source_files = [f for f in os.listdir(src_dir) if f.endswith((".cpp", ".c", ".cc", ".S"))]
                if source_files:
                    mod_name = os.path.basename(mod_dir)
                    variant_dir = os.path.join("$BUILD_DIR", "ggg_modules", mod_name)
                    try:
                        env.BuildSources(variant_dir, src_dir)
                        print(f"[+] Registered sources for active module [{mod_name}]: {len(source_files)} files")
                    except Exception as e:
                        print(f"[!] Warning: BuildSources for {mod_name} ({e})")

        # In native environment, also build inactive plugin and lib sources so unit test suites can link all modules
        if env.get("PIOPLATFORM") == "native":
            for s_root in search_roots:
                if os.path.isdir(s_root):
                    for item in os.listdir(s_root):
                        item_path = os.path.join(s_root, item)
                        if os.path.isdir(item_path) and not any(os.path.samefile(item_path, m[0]) for m in active_modules if os.path.exists(m[0])):
                            src_cand = os.path.join(item_path, "src")
                            if os.path.isdir(src_cand):
                                src_files = [f for f in os.listdir(src_cand) if f.endswith((".cpp", ".c", ".cc"))]
                                if src_files:
                                    var_dir = os.path.join("$BUILD_DIR", "ggg_modules", item)
                                    try:
                                        env.BuildSources(var_dir, src_cand)
                                        print(f"[+] Registered native test sources for module [{item}]: {len(src_files)} files")
                                    except Exception:
                                        pass

        # Register menuconfig custom target
        try:
            if "__PIO_TARGETS" not in env or "menuconfig" not in env["__PIO_TARGETS"]:
                env.AddCustomTarget(
                    name="menuconfig",
                    dependencies=None,
                    actions=["menuconfig"],
                    title="Run Kconfig Menu",
                    description="Launch interactive GGG configuration menu"
                )
        except Exception:
            pass
    else:
        print("\n[*] Standalone execution summary:")
        print(f"    - Include paths: {len(all_cpp_paths)}")
        print(f"    - Injected libs: {libs_to_inject}")
    
    print("==================================================\n")

if __name__ == "__main__" or IS_PIO_ENV:
    main()
