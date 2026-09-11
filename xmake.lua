set_project("Wi")

set_version("9.0.0-beta")
set_description("The Wi programming language")
set_license("MIT")

set_languages("c99")
set_warnings("all", "extra", "pedantic")

option("werror")
    set_description("Error on warnings (enable -Werror)")
    set_default(true)
    set_showmenu(true)
option_end()

if has_config("werror") then
    set_warnings("error")
end

-- wi doesn't really support macosx but it won't hurt to add the check here
-- "doesn't support" is a big stretch too since only problem on macosx is no foreign library loading
if is_plat("linux", "macosx") then
    add_requires("readline", {optional = true})
end

function common()
    if is_mode("debug") then
        add_cflags("-g -fno-omit-frame-pointer")
        set_optimize("none")
        set_symbols("debug")
    elseif is_mode("release") then
        add_cflags("-flto -fno-stack-protector -fno-common")
        set_optimize("fastest")
        set_strip("all")
    end

    add_cflags("-Wconversion")
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
