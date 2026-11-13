// T26: runs the SAME conformance importer component in wasmtime-dotnet (the Windows app's host).
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using Wasmtime;
using Wasmtime.Components;

var dir = args[0];
var bytes = File.ReadAllBytes(Path.Combine(dir, "conformance-importer.wasm"));
int failures = 0;
void Check(string name, bool ok, string detail = "") { Console.WriteLine($"{(ok ? "PASS" : "FAIL")} {name} {detail}"); if (!ok) failures++; }

// 1. Inspect imports/exports without instantiating (not wrapped by wasmtime-dotnet: P/Invoke).
var (imports, exports) = Inspect.Run(bytes);
Check("inspect imports", imports.SequenceEqual(new[] { "easepass:plugin/log@1.0.0", "easepass:plugin/types@1.0.0", "easepass:plugin/file-picker@1.0.0" }), string.Join(", ", imports));
Check("inspect exports", exports.SequenceEqual(new[] { "easepass:plugin/importer@1.0.0" }), string.Join(", ", exports));
var (badImports, _) = Inspect.Run(File.ReadAllBytes(Path.Combine(dir, "negative", "forbidden-import.wasm")));
Check("forbidden import visible", badImports.Contains("wasi:filesystem/preopens@0.2.0"));

using var engine = new Engine(new Config().WithEpochInterruption(true));
using var component = Component.FromBytes(engine, bytes);
var log = new List<string>();
byte[]? pickedFile = null;

(Store, ComponentInstance) Load(bool approveFilePicker, long? memory = null, TimeSpan? timeout = null) {
    var store = new Store(engine);
    store.SetLimits(memory, null, null, null, null);
    var deadline = Stopwatch.StartNew();
    var limit = timeout ?? TimeSpan.FromSeconds(30);
    store.SetEpochDeadline(1);
    store.SetEpochDeadlineCallback(_ => deadline.Elapsed > limit ? throw new TimeoutException("plugin call timed out") : 1UL);
    using var linker = new ComponentLinker(engine);
    using (var root = linker.Root()) {
        using (var types = root.AddInstance("easepass:plugin/types@1.0.0")) { }
        using (var logInstance = root.AddInstance("easepass:plugin/log@1.0.0"))
            logInstance.DefineFunction("log", (arguments, results) => log.Add($"{arguments[0].AsEnum()}:{arguments[1].AsString()}"));
        if (approveFilePicker) {
            using var picker = root.AddInstance("easepass:plugin/file-picker@1.0.0");
            picker.DefineFunction("pick-file", (arguments, results) => {
                var file = pickedFile == null ? ComponentValue.None() : ComponentValue.Some(ComponentValue.Record(new[] {
                    KeyValuePair.Create("name", ComponentValue.String("export.csv")),
                    KeyValuePair.Create("data", ComponentValue.List(pickedFile.Select(ComponentValue.U8).ToArray())),
                }));
                results[0] = ComponentValue.Ok(file);
            });
        }
    }
    return (store, linker.Instantiate(store, component));
}

const string Importer = "easepass:plugin/importer@1.0.0";
ComponentValue Import(ComponentInstance instance, string source) =>
    instance.GetFunction(Importer, "import-passwords")!.Call(new[] { ComponentValue.String(source) })!;

// 2. Unapproved import: instantiation fails.
try { Load(approveFilePicker: false); Check("unapproved import fails to instantiate", false); }
catch (WasmtimeException e) { Check("unapproved import fails to instantiate", e.Message.Contains("file-picker"), e.Message.Split('\n')[0]); }

// 3. Normal calls.
{
    var (store, instance) = Load(true);
    var sources = instance.GetFunction(Importer, "sources")!.Call(Array.Empty<ComponentValue>())!;
    Check("sources", sources.AsList().Select(s => s.Field("id").AsString()).SequenceEqual(new[] { "fixed", "csv-file" }));
    var result = Import(instance, "fixed");
    var items = result.Payload!.AsList();
    Check("import fixed", result.IsOk && items.Count == 2 && items[0].Field("display-name").AsString() == "Example"
        && items[1].Field("display-name").AsString() == "Ünïcödé ✓" && items[1].Field("password").AsString() == "pässwörd-🔑"
        && items[1].Field("totp").Payload!.Field("secret").AsString() == "JBSWY3DPEHPK3PXP"
        && items[1].Field("totp").Payload!.Field("algorithm").AsEnum() == "sha1" && !items[0].Field("totp").IsSome);
    Check("log", log.SequenceEqual(new[] { "info:conformance-importer: importing fixed items" }));

    pickedFile = Encoding.UTF8.GetBytes("Mail;carol;s3cret\nBänk;dave;geld €\n");
    var csv = Import(instance, "csv-file");
    Check("import csv through file picker", csv.IsOk && csv.Payload!.AsList().Count == 2 && csv.Payload!.AsList()[1].Field("password").AsString() == "geld €");
    pickedFile = null;
    var cancelled = Import(instance, "csv-file");
    Check("file picker cancelled", !cancelled.IsOk && cancelled.Payload!.Field("code").AsEnum() == "cancelled");
    var error = Import(instance, "test-error");
    Check("plugin error", !error.IsOk && error.Payload!.Field("code").AsEnum() == "unavailable" && error.Payload!.Field("message").AsString() == "requested test error");
    store.Dispose();
}

// 4. Trap, timeout, memory cap: errors, not crashes.
{
    var (store, instance) = Load(true);
    try { Import(instance, "test-trap"); Check("trap", false); }
    catch (WasmtimeException e) { Check("trap", e.Message.Contains("unreachable"), ""); }
    store.Dispose();
}
{
    var ticker = new CancellationTokenSource();
    var thread = new Thread(() => { while (!ticker.IsCancellationRequested) { Thread.Sleep(20); engine.IncrementEpoch(); } });
    thread.Start();
    var (store, instance) = Load(true, timeout: TimeSpan.FromMilliseconds(200));
    var watch = Stopwatch.StartNew();
    try { Import(instance, "test-infinite-loop"); Check("infinite loop interrupted", false); }
    catch (WasmtimeException e) { Check("infinite loop interrupted", watch.ElapsedMilliseconds is >= 200 and < 5000, $"{watch.ElapsedMilliseconds} ms, inner: {e.InnerException?.GetType().Name}"); }
    store.Dispose();

    var (store2, instance2) = Load(true, memory: 16 * 1024 * 1024, timeout: TimeSpan.FromSeconds(60));
    watch.Restart();
    try { Import(instance2, "test-allocate"); Check("memory cap", false); }
    catch (WasmtimeException e) { Check("memory cap", watch.ElapsedMilliseconds < 30000 && e.InnerException is not TimeoutException, $"{watch.ElapsedMilliseconds} ms"); }
    store2.Dispose();
    ticker.Cancel(); thread.Join();
}
{
    var (store, instance) = Load(true);
    Check("host alive, fresh instance works", Import(instance, "fixed").Payload!.AsList().Count == 2);
    store.Dispose();
}
Console.WriteLine(failures == 0 ? "ALL PASSED" : $"{failures} FAILED");
return failures;

static class Inspect {
    const string Lib = "wasmtime";
    [DllImport(Lib)] static extern IntPtr wasm_engine_new();
    [DllImport(Lib)] static extern void wasm_engine_delete(IntPtr engine);
    [DllImport(Lib)] static extern IntPtr wasmtime_component_new(IntPtr engine, byte[] buf, nuint len, out IntPtr component);
    [DllImport(Lib)] static extern void wasmtime_component_delete(IntPtr component);
    [DllImport(Lib)] static extern IntPtr wasmtime_component_type(IntPtr component);
    [DllImport(Lib)] static extern void wasmtime_component_type_delete(IntPtr type);
    [DllImport(Lib)] static extern nuint wasmtime_component_type_import_count(IntPtr type, IntPtr engine);
    [DllImport(Lib)] static extern nuint wasmtime_component_type_export_count(IntPtr type, IntPtr engine);
    [DllImport(Lib)] static extern bool wasmtime_component_type_import_nth(IntPtr type, IntPtr engine, nuint nth, out IntPtr name, out nuint nameLen, out Item item);
    [DllImport(Lib)] static extern bool wasmtime_component_type_export_nth(IntPtr type, IntPtr engine, nuint nth, out IntPtr name, out nuint nameLen, out Item item);
    [DllImport(Lib)] static extern void wasmtime_component_item_delete(ref Item item);
    [StructLayout(LayoutKind.Sequential)] struct Item { public byte Kind; public IntPtr A; public IntPtr B; public IntPtr C; }

    public static (List<string> Imports, List<string> Exports) Run(byte[] bytes) {
        var engine = wasm_engine_new();
        if (wasmtime_component_new(engine, bytes, (nuint)bytes.Length, out var component) != IntPtr.Zero) throw new Exception("not a component");
        var type = wasmtime_component_type(component);
        var imports = new List<string>(); var exports = new List<string>();
        for (nuint i = 0; i < wasmtime_component_type_import_count(type, engine); i++)
            if (wasmtime_component_type_import_nth(type, engine, i, out var name, out var len, out var item)) { imports.Add(Marshal.PtrToStringUTF8(name, (int)len)); wasmtime_component_item_delete(ref item); }
        for (nuint i = 0; i < wasmtime_component_type_export_count(type, engine); i++)
            if (wasmtime_component_type_export_nth(type, engine, i, out var name, out var len, out var item)) { exports.Add(Marshal.PtrToStringUTF8(name, (int)len)); wasmtime_component_item_delete(ref item); }
        wasmtime_component_type_delete(type); wasmtime_component_delete(component); wasm_engine_delete(engine);
        return (imports, exports);
    }
}
