package maxpower.hash;

import java.util.ArrayList;
import java.util.List;

import maxpower.hash.mem.MemInterface;
import maxpower.hash.mem.MemInterface.MemType;

import com.maxeler.maxcompiler.v2.kernelcompiler.Kernel;
import com.maxeler.maxcompiler.v2.kernelcompiler.KernelLib;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.KernelObject;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.KernelType;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFEVar;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.composite.DFEStructType;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.composite.DFEStructType.StructFieldType;
import com.maxeler.maxcompiler.v2.managers.custom.CustomManager;
import com.maxeler.maxcompiler.v2.managers.custom.blocks.KernelBlock;

public abstract class MaxHash<T extends KernelObject<T>> extends
		KernelLib {

	private final String m_name;
	private final DFEVar m_key;
	private final KernelType<T> m_valueType;
	private final boolean m_validateResults;
	private final CustomManager m_manager;

	private static final int MAPPED_MEM_ENTRY_WIDTH = 64;

	private final boolean DEBUG = false;

	public void simPrintf(DFEVar condition, String format, Object ... args) {
		if (DEBUG)
			debug.simPrintf(condition, format, args);
	}

	public void simPrintString(DFEVar condition, DFEVar var) {
		if (DEBUG)
			for (int bit = 0; bit < var.getType().getTotalBits(); bit += 8) {
				DFEVar inChar = var.slice(bit, 8);
				for (char testChar = 48; testChar < 123; testChar++)
					debug.simPrintf(condition & (inChar === testChar),
							Character.toString(testChar));
			}
	}

	/* Wire up a MaxHash instance in a CustomManager. */
	public static void connectKernelMemoryStreams(CustomManager m, Kernel k, KernelBlock kb) {
		for (MaxHash<?> ht : MaxHashFactory.getHashTables(k)) {
			/* Connect hashing block to memory for table lookups. */
			for (MemInterface mi : ht.getMemInterfaces()) {
				mi.connectKernelMemoryStreams(m, kb);
			}
		}
	}

	public static void setupHostMemoryStreams(CustomManager m) {
		for (MaxHash<?> ht : MaxHashFactory.getHashTables(m)) {
			/* Connect hashing block to LMem for table lookups. */
			for (MemInterface mi : ht.getMemInterfaces()) {
				mi.setupHostMemoryStreams(m);
			}
		}
	}

	DFEStructType getOutputStructType() {
		List<StructFieldType> fields = new ArrayList<StructFieldType>();
		fields.add(DFEStructType.sft("valid", dfeBool()));

		if (m_validateResults)
			fields.add(DFEStructType.sft("key", m_key.getType()));

		fields.add(DFEStructType.sft("value", m_valueType));

		return new DFEStructType(fields.toArray(new StructFieldType[0]));
	}

	MaxHash(MaxHashParameters<T> params, KernelLib owner, DFEVar key, DFEVar keyValid) {
		super(owner);

		if (!key.getType().equals(params.getKeyType()))
			throw new MaxHashException("Hash key has a different type to that specified using setKeyType().");

		m_valueType = params.getValueType();
		m_key = key;
		m_validateResults = params.isValidateResults();
		m_name = params.getName();
		m_manager = params.getManager();

		if (params.getValuesMemType() == MemType.FMEM && getOutputStructType().getTotalBits() > MAPPED_MEM_ENTRY_WIDTH) {
			throw new MaxHashException("The specified MaxHash parameters "
					+ "lead to a memory entry width that is greater than "
					+ "the currently supported maximum of 64 bits for "
					+ "FMem-backed MaxHash instances.");
		}

		addMaxFileConstant("IsPresent", 1);
		addMaxFileConstant("KeyWidth", params.getKeyType().getTotalBits());
		addMaxFileConstant("ValidateResults", params.isValidateResults() ? 1 : 0);
	}

	public String getName() {
		return m_name;
	}

	public String getFullName() {
		return getKernel().getName() + "_" + m_name;
	}

	@Override
	public CustomManager getManager() {
		return m_manager;
	}

	public void addMaxFileConstant(String name, int value) {
		getManager().addMaxFileConstant(getFullName() + "_" + name, value);
	}

	public void addMaxFileStringConstant(String name, String value) {
		getManager().addMaxFileStringConstant(getFullName() + "_" + name, value);
	}

	public abstract T get();

	public abstract DFEVar getIndex();

	public abstract DFEVar containsKey();

	protected abstract int getMaxBucketEntries();

	public abstract List<MemInterface> getMemInterfaces();
}
