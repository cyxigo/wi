local flags = "-march=native -Ofast -fno-stack-protector -fno-common -std=c11 -Wall -Werror -Wconversion -Wsign-conversion -Wfloat-conversion"

target("wi_shared")
    set_kind("shared")
    set_group("libs")
    set_filename("wi.dll")

    add_cflags(flags)
    add_headerfiles("src/core/*.h", "src/std/*.h")
    add_files("src/core/*.c", "src/std/*.c")

    after_build(function (target) 
        os.cp(target:targetfile(), os.projectdir())
    end)

target("wi")
    set_kind("binary")
    set_group("apps")

    add_cflags(flags)
    add_headerfiles("src/core/*.h", "src/std/*.h")
    add_files("src/core/*.c", "src/std/*.c")  

    after_build(function (target) 
        os.cp(target:targetfile(), os.projectdir())
    end)
