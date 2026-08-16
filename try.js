var Module = {
    print: function (text) {
        document.getElementById("output").textContent += text + "\n";
    },
    printErr: function (text) {
        document.getElementById("output").textContent += text + "\n";
    },
    onAbort: function (what) {
        var run = document.getElementById("run");
        run.textContent = "Failed to load";
        document.getElementById("output").textContent = "wasm load failed: " + what;
    },
    onRuntimeInitialized: function () {
        var run = document.getElementById("run");
        run.disabled = false;
        run.textContent = "Run";
    },
};

Module.stdin = function () {
    return null;
};

document.addEventListener("DOMContentLoaded", function () {
    var code = document.getElementById("code");
    var run = document.getElementById("run");
    var output = document.getElementById("output");

    run.addEventListener("click", function () {
        output.textContent = "";
        Module.ccall("wi_wasm_init", null, [], []);
        var result = Module.ccall("wi_wasm_run", "number", ["string"], [code.value]);

        if (result !== 0 /* WI_RUN_OK */) {
            output.textContent += Module.ccall("wi_wasm_get_error", "string", [], []);
        }
    });
});
