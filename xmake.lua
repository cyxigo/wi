set_project("Wi")

set_version("9.1.0-beta")
set_description("The Wi programming language")
set_license("MIT")

set_languages("c99")
set_warnings("all", "extra", "pedantic")

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

option("werror")
    set_description("Error on warnings (enable -Werror)")
    set_default(false)
    set_showmenu(true)
option_end()

if has_config("werror") then
    set_warnings("error")
end

add_requires("readline", {optional = true})

-- check if our toolchain can accept gnu flags like -g or -flto
-- on anything other than god forsaken windows we just return true
function is_gnu_compatible()
    if not is_plat("windows") then
        return true -- linux/macosx/bsd default toolchains are always gcc/clang
    end

    local toolchain = get_config("toolchain")
    return toolchain == "mingw" or toolchain == "clang" or toolchain == "gcc"
end

function common()
    if is_mode("debug") then
        if is_gnu_compatible() then
            add_cflags("-fno-omit-frame-pointer")
        end

        set_optimize("none")
        set_symbols("debug")
    elseif is_mode("release") then
        if is_gnu_compatible() then
            add_cflags("-flto", "-fno-stack-protector", "-fno-common")
        end

        set_optimize("fastest")
        set_strip("all")
    end

    if is_gnu_compatible() then
        add_cflags("-Wconversion")
    end

    add_headerfiles("src/core/*.h", "src/std/*.h", "src/stm/*.h")
    add_files("src/core/*.c", "src/std/*.c", "src/stm/*.c")
    add_includedirs("src/core", "src/std", "src/stm", "include")

    set_targetdir("bin")
end

target("wi_shared")
    set_enabled(not is_plat("wasm"))
    set_kind("shared")
    set_group("libs")
    set_basename("wi")

    common()

    if is_gnu_compatible() then
        add_cflags("-fvisibility=hidden", {force = true})
    end

target("wi")
    set_enabled(not is_plat("wasm"))
    set_kind("binary")
    set_group("apps")

    common()

    if is_plat("linux") then
        add_ldflags("-rdynamic", {force = true})
    end

    if is_plat("windows") then 
        add_files("windows/wi.rc")
    end

    if has_package("readline") then
        add_defines("WI_USE_READLINE")
        add_packages("readline")
    end

target("wi_wasm")
    set_enabled(is_plat("wasm"))
    set_kind("binary")
    set_group("web")
    set_filename("wi.js")

    if is_mode("debug") then
        set_optimize("none")
        add_ldflags("-sASSERTIONS=1", "-sSAFE_HEAP=1", "-gsource-map", {force = true})
    elseif is_mode("release") then
        set_optimize("fastest")
    end
    
    add_ldflags(
        "-sEXPORTED_FUNCTIONS=['_wi_wasm_init','_wi_wasm_run']",
        "-sEXPORTED_RUNTIME_METHODS=['ccall']",
        "-sALLOW_MEMORY_GROWTH=1", 
        {force = true}
    )

    add_files("src/core/*.c|wi.c", "src/std/*.c", "src/stm/*.c", "src/wasm/wi_wasm.c")
    add_includedirs("src/core", "src/std", "src/stm", "include")

    set_targetdir("bin")
