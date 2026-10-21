includes("@builtin/xpack")

set_project("Wi")
set_version("9.8.0-beta")
set_description("The Wi programming language")
set_license("MIT")

-- i hate msvc
function is_msvc()
    return is_plat("windows") and get_config("toolchain") == "msvc"
end

-- i hate msvc
set_languages(is_msvc() and "c11" or "c99")

-- NaN boxing is not nearly a portable thingy so Wi has an option to use union tagging
-- of course, it makes Wi slower overall. so union tagging is opt-in
option("union")
    set_description("Use union tagging instead of NaN boxing for value representation")
    set_default(false)
    set_showmenu(true)
option_end()

if has_config("union") then
    add_defines("WI_UNION_TAGGING")
end

-- i personally enable this, but some compilers can give warnings that are simply wrong or that i didn't account for
-- so it's more logical to keep this optional and opt-in
option("werror")
    set_description("Error on warnings (enable -Werror)")
    set_default(false)
    set_showmenu(true)
option_end()

set_policy("build.warning", true)

if has_config("werror") then
    set_warnings("all", "extra", "pedantic", "error")
else
    set_warnings("all", "extra", "pedantic")
end

-- wi uses horrid winapi on windows with ReadConsoleW and other winapi horror functions
if not is_plat("windows") and not get_config("cross") then
    add_requires("readline", { optional = true })
end

-- check if our toolchain can accept gnu flags like -fno-common -fno-stack-protector
-- on anything other than god forsaken windows we just return true
function is_gnu()
    if not is_plat("windows") then
        return true -- linux/macosx/bsd default toolchains are always gcc/clang
    end

    local toolchain = get_config("toolchain")
    return toolchain == "mingw" or toolchain == "clang" or toolchain == "gcc"
end

-- compiler flags shared by every native (non-wasm) target
function set_flags()
    if is_mode("debug") then
        if is_gnu() then
            add_cflags("-fno-omit-frame-pointer")
        end

        set_optimize("none")
        set_symbols("debug")
    elseif is_mode("release") then
        if is_gnu() then
            add_cflags("-fno-stack-protector", "-fno-common")
        end

        if not is_arch("x86", "i386", "arm64") then
            set_policy("build.optimization.lto", true)
        end

        set_optimize("fastest")
        set_strip("all")
    end

    if has_package("readline") then
        add_defines("WI_USE_READLINE")
        add_packages("readline")
    end

    if not is_plat("windows") then
        add_defines("_FILE_OFFSET_BITS=64")
    end

    -- i hate msvc
    if is_msvc() then
        add_defines("_CRT_SECURE_NO_WARNINGS")
        add_cxflags("-wd4324", "-wd4709", { force = true })
        -- thank you windows for cryptic ass error codes that i need to explain in comments:
        -- C4324: structure was padded (triggers on wi_recovery)
        -- C4709: comma operator in subscript (triggers on _READ_SHORT in _state_interpreter_loop)
    end

    if is_gnu() then
        add_cflags("-Wconversion")
    end
end

-- set target directory to bin and add every source/header file excluding wi.c and wi_wasm.c
function set_src()
    -- x64 and wasm go to bin directly, any other architecture to bin/<arch>
    if is_plat("wasm") or is_arch("x64", "x86_64") then
        set_targetdir("bin")
    else
        set_targetdir("bin/$(arch)")
    end

    -- install = false means it won't be included in the xpack .zip
    add_headerfiles("src/core/*.h", "src/std/*.h", "src/stm/*.h", { install = false })
    add_headerfiles("include/*.h") -- and this will be included
    add_files("src/core/*.c|wi.c", "src/std/*.c", "src/stm/*.c")
    add_includedirs("src/core", "src/std", "src/stm", "include")
end

target("wi")
    set_enabled(not is_plat("wasm"))
    set_kind("binary")
    set_group("apps")

    set_flags()
    set_src()
    add_files("src/core/wi.c")
    
    if is_plat("linux") then
        add_cflags("-fvisibility=hidden", { force = true })
        add_ldflags("-rdynamic", { force = true })
    end

    if is_plat("windows") then
        add_files("windows/wi.rc")
        add_syslinks("shell32")
    end

    if is_msvc() then
        add_ldflags("/IMPLIB:bin/wi_exe.lib", { force = true })
    end
target_end()

target("wi_shared")
    set_enabled(not is_plat("wasm"))
    set_kind("shared")
    set_group("libs")
    set_basename("wi")

    set_flags()
    set_src()

    if is_plat("windows") then
        add_defines("WI_DLL")
    end

    if is_gnu() then
        add_cflags("-fvisibility=hidden", { force = true })
    end
target_end()

target("wi_wasm")
    set_enabled(is_plat("wasm"))
    set_kind("binary")
    set_group("web")
    set_filename("wi.js")

    -- never seen these warnings before but now they are everywhere so
    -- we make emcc SHUT UP!!!!
    add_cflags("-Wno-gnu-line-marker", "-Wno-unused-command-line-argument")

    if is_mode("debug") then
        set_optimize("none")
        add_ldflags("-sASSERTIONS=1", "-sSAFE_HEAP=1", "-gsource-map", { force = true })
    elseif is_mode("release") then
        set_optimize("fastest")
    end

    add_ldflags(
        "-sEXPORTED_FUNCTIONS=['_wi_wasm_init','_wi_wasm_run']",
        "-sEXPORTED_RUNTIME_METHODS=['ccall']",
        "-sALLOW_MEMORY_GROWTH=1", 
        { force = true }
    )

    set_src()
    add_files("src/wasm/wi_wasm.c")
target_end()

-- this is such a blessing thank you xmake
xpack("wi")
    set_formats("zip")
    
    add_targets("wi", "wi_shared")
    set_basename("wi-$(plat)-$(arch)")

    add_installfiles("include/wi.hpp", { prefixdir = "include" })
    add_installfiles("LICENSE", "CHANGELOG.md", "README.md")
xpack_end()
