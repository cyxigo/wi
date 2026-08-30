var Module = {
    print: (text) => {
        document.getElementById("output").textContent += text;
    },
    printErr: (text) => {
        var output = document.getElementById("output");
        output.textContent += text;
        output.style.color = "#cc0000";
    },
    onAbort: (what) => {
        var run = document.getElementById("run");
        run.textContent = "Failed to load";
        document.getElementById("output").textContent = "wasm load failed: " + what;
    },
    onRuntimeInitialized: () => {
        var run = document.getElementById("run");
        run.disabled = false;
        run.textContent = "Run";
    },
};

Module.stdin = () => {
    return null;
};

document.addEventListener("DOMContentLoaded", () => {
    var code = document.getElementById("code");
    var run = document.getElementById("run");
    var output = document.getElementById("output");

    run.addEventListener("click", () => {
        output.textContent = "";
        output.style.color = "#3e3e3e";
        Module.ccall("wi_wasm_init", null, [], []);
        Module.ccall("wi_wasm_run", "number", ["string"], [code.value]);
    });
});

var exampleHello = () => {
    var code = document.getElementById("code");
    code.value = `puts("Hello World!");`;
};

var exampleLoop = () => {
    var code = document.getElementById("code");
    code.value = `for (i := 0; i < 5; i = i + 1) {
    puts("Looping... i: \${i}");
}`;
};

var exampleObject = () => {
    var code = document.getElementById("code");
    code.value = `obj_person := object {
    name: "";
    greet: |self| => puts("Hi \${self.name}!");
};

bob := new obj_person {
    name: "Bob";
};

bob->greet();`;
};

var examplePipeline = () => {
    var code = document.getElementById("code");
    code.value = `numbers := [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
numbers
    ->where(|x| => x % 2 == 0) // Where... only evens
    ->select(|x| => x ** 2) // Select... squares
    ->each(|x| => puts("Number: \${x}")); // Each... puts!`;
};

var exampleMerging = () => {
    var code = document.getElementById("code");
    code.value = `obj_contact := object {
    email: "";
    phone: "";
};

obj_address := object {
    street: "";
    city: "";
};

obj_person := new obj_contact, obj_address {
    name: "";
    show_info: |self| => {
        puts("\${self.name} lives in \${self.city}, email: \${self.email}");
    };
};

alice := new obj_person {
    name: "Alice";
    email: "alice@work.com";
    city:"New York";
};

alice->show_info();`;
};
