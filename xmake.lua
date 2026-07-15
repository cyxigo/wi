set_project("Wi")

set_version("1.0.0")
set_description("The Wi programming language")

function defaults()
    add_languages("c11")
    add_cflags("-fno-stack-protector -fno-common -Wconversion -Wsign-conversion -Wfloat-conversion")
    set_optimize("faster")
    set_warnings("all", "error")

    add_headerfiles("src/core/*.h", "src/std/*.h")
    add_files("src/core/*.c", "src/std/*.c")

    set_targetdir("bin")
end

target("wi_shared")
    set_kind("shared")
    set_group("libs")
    set_basename("wi")
    defaults()

target("wi")
    set_kind("binary")
    set_group("apps")
    defaults()

    if is_host("windows") then 
        add_files("wi.rc")
    end 
