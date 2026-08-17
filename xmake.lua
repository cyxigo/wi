set_project("Wi")

set_version("1.0.0")
set_description("The Wi programming language")
set_license("MIT")

set_languages("c99")
set_warnings("everything", "error", "pedantic")

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
    
    add_headerfiles("src/core/*.h", "src/std/*.h")
    add_files("src/core/*.c", "src/std/*.c")
    add_includedirs("src/core", "src/std")

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
 
    if is_plat("windows") then 
        add_files("wi.rc")
    elseif is_plat("linux") and os.isfile("/usr/include/readline/readline.h") then
        add_defines("WI_USE_READLINE")
        add_links("readline")
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
        "-sEXPORTED_FUNCTIONS=['_wi_wasm_init','_wi_wasm_run','_wi_wasm_get_error']",
        "-sEXPORTED_RUNTIME_METHODS=['ccall']",
        "-sALLOW_MEMORY_GROWTH=1", 
        {force = true}
    )

    add_files("src/core/*.c|wi.c", "src/std/*.c", "src/wasm/wi_wasm.c")
    add_includedirs("src/core", "src/std", "src/include")

    set_targetdir("bin")
