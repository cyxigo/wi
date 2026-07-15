local cflags = "-O2 -fno-stack-protector -fno-common -std=c11 -Wall -Werror -Wconversion -Wsign-conversion -Wfloat-conversion"

target("wi_shared")
    set_kind("shared")
    set_group("libs")
    set_basename("wi")
    set_targetdir("bin") 

    add_cflags(cflags)
    add_headerfiles("src/core/*.h", "src/std/*.h")
    add_files("src/core/*.c", "src/std/*.c")

target("wi")
    set_kind("binary")
    set_group("apps")
    set_targetdir("bin")

    add_cflags(cflags)
    add_headerfiles("src/core/*.h", "src/std/*.h")
    add_files("src/core/*.c", "src/std/*.c")  

    if is_host("windows") then 
        add_files("wi.rc")
    end 
