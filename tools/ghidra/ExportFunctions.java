// Export selected functions and incoming references without modifying the program.
// Arguments: output path, then hexadecimal addresses.
// @category LostOdyssey
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;

public class ExportFunctions extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("output and addresses required");
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0], StandardCharsets.UTF_8)) {
            for (int i = 1; i < args.length; ++i) {
                Address at = toAddr(Long.parseUnsignedLong(args[i].replace("0x", ""), 16));
                Function f = getFunctionContaining(at);
                if (f == null) f = getFunctionAt(at);
                out.println("\n// Requested " + at + ": " + (f == null ? "NO FUNCTION" : f.getName()));
                if (f == null) continue;
                out.println("// Entry " + f.getEntryPoint());
                for (Reference ref : getReferencesTo(f.getEntryPoint())) {
                    Function caller = getFunctionContaining(ref.getFromAddress());
                    out.println("// Reference " + ref.getFromAddress() + " " + (caller == null ? "data" : caller.getName()));
                }
                DecompileResults result = decompiler.decompileFunction(f, 45, monitor);
                out.println(result.decompileCompleted() ? result.getDecompiledFunction().getC() : result.getErrorMessage());
                out.flush();
            }
        } finally { decompiler.dispose(); }
    }
}
