// Read-only cross references for selected addresses. Arguments: output, addresses.
// @category LostOdyssey
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
public class ExportReferences extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(args[0])) {
            for (int i=1;i<args.length;i++) {
                Address a=toAddr(Long.parseUnsignedLong(args[i].replace("0x",""),16));
                out.println("Address " + a);
                for (Reference r:getReferencesTo(a)) {
                    Function f=getFunctionContaining(r.getFromAddress());
                    out.println(r.getFromAddress()+" "+(f==null?"data":f.getEntryPoint()+" "+f.getName()));
                }
            }
        }
    }
}
