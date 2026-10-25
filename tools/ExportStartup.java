// Export selected native functions for the private Sonic port analysis.
// @category Sonic
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.io.File;
import java.util.LinkedHashMap;
import java.nio.file.Files;
import ghidra.program.model.address.Address;

public class ExportStartup extends GhidraScript {
    private void callers(Address address, int depth, LinkedHashMap<String, Function> targets) {
        if (depth == 0) return;
        for (var ref : getReferencesTo(address)) {
            Function f = getFunctionContaining(ref.getFromAddress());
            if (f == null) callers(ref.getFromAddress(), depth - 1, targets);
            else if (f.isThunk()) callers(f.getEntryPoint(), depth - 1, targets);
            else targets.put("caller_" + f.getEntryPoint(), f);
        }
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        File output = new File(args[0]);
        output.mkdirs();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long imageBase = currentProgram.getImageBase().getOffset();
        println("Ghidra image base: " + Long.toHexString(imageBase));
        LinkedHashMap<String, Function> targets = new LinkedHashMap<>();
        File extra = new File(output.getParentFile(), "analysis/extra_exports.tsv");
        if (extra.exists()) {
            for (String line : Files.readAllLines(extra.toPath())) {
                if (line.isBlank()) continue;
                String[] pair = line.split(":", 2);
                var address = toAddr(imageBase + Long.parseUnsignedLong(pair[1], 16));
                var f = getFunctionAt(address);
                if (f == null) { disassemble(address); f = createFunction(address, pair[0]); }
                if (f != null) targets.put(pair[0], f);
            }
        }
        for (int i = 1; i < args.length; i++) {
            String[] pair = args[i].split(":", 2);
            var address = toAddr(imageBase + Long.parseUnsignedLong(pair[1], 16));
            Function function = getFunctionAt(address);
            if (function == null) {
                disassemble(address);
                function = createFunction(address, pair[0]);
            }
            if (function == null) { printerr("No function at " + address); continue; }
            targets.put(pair[0], function);
        }
        for (String name : new String[]{"mmap", "mprotect", "statfs", "uname", "nice", "pthread_attr_setstack"}) {
            var symbols = currentProgram.getSymbolTable().getSymbols(name);
            while (symbols.hasNext()) callers(symbols.next().getAddress(), 4, targets);
        }
        for (long elfAddress : new long[]{0x267fc, 0x1871c, 0x3c104}) {
            var address = toAddr(imageBase + elfAddress);
            var f = getFunctionAt(address);
            if (f != null) targets.put("startup_" + Long.toHexString(elfAddress), f);
        }
        File callsites = new File(output.getParentFile(), "analysis/import_callers.tsv");
        if (callsites.exists()) {
            for (String line : Files.readAllLines(callsites.toPath())) {
                if (line.isBlank()) continue;
                String[] pair = line.split(":", 2);
                var f = getFunctionContaining(toAddr(imageBase + Long.parseUnsignedLong(pair[1], 16)));
                if (f != null) targets.put("uses_" + pair[0] + "_" + pair[1], f);
            }
        }
        for (var entry : targets.entrySet()) {
            var function = entry.getValue();
            var address = function.getEntryPoint();
            DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
            try (PrintWriter writer = new PrintWriter(new File(output, entry.getKey() + ".c"))) {
                writer.println("/* ELF virtual address " + Long.toHexString(address.getOffset() - imageBase) + "; Ghidra address " + address +
                               "; decompiler output, verify types against JNI/Java. */");
                if (result.decompileCompleted()) writer.print(result.getDecompiledFunction().getC());
                else writer.println("/* Failed: " + result.getErrorMessage() + " */");
            }
            println("Exported " + entry.getKey() + " at " + address);
        }
        decompiler.dispose();
    }
}
