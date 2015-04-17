package maxpower.hash.mem;

import java.util.ArrayList;
import java.util.List;

import maxpower.hash.MaxHash;
import maxpower.hash.MaxHashException;

import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFEVar;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.composite.DFEStruct;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.composite.DFEStructType;
import com.maxeler.maxcompiler.v2.managers.custom.CustomManager;
import com.maxeler.maxcompiler.v2.managers.custom.blocks.KernelBlock;

public abstract class MemInterface {

	public enum MemType { UNDEFINED, FMEM, DEEP_FMEM, LMEM, QDR }
	public enum Buffer { A, B }

	private final MaxHash<?> m_owner;
	private final String m_memName;
	private final DFEStructType m_structType;
	private final int m_numEntries;
	private final boolean m_isDoubleBuffered;

	public static MemInterface create(MaxHash<?> owner,
			MemType memType,
			String memName,
			DFEStructType structType,
			int baseAddressBursts,
			int numEntries,
			boolean isDoubleBuffered,
			DFEVar bufferSelect) {

		if (memType == MemType.LMEM) {
			return new LMemInterface(owner, memName,
					structType, numEntries, isDoubleBuffered,
					baseAddressBursts);
		} else if (memType == MemType.QDR) {
			return new QDRInterface(owner, memName,
					structType, numEntries, isDoubleBuffered,
					baseAddressBursts);
		} else if (memType == MemType.FMEM) {
			return new FMemInterface(owner, memName,
					structType, numEntries, isDoubleBuffered);
		} else if (memType == MemType.DEEP_FMEM) {
			return new DeepFMemInterface(owner, memName,
					structType, numEntries, isDoubleBuffered,
					bufferSelect);
		} else {
			throw new MaxHashException("Invalid memory type.");
		}
	}

	public MemInterface(MaxHash<?> owner, String memName, DFEStructType structType,
			int numEntries, boolean isDoubleBuffered) {
		m_owner = owner;
		m_memName = memName;
		m_structType = structType;
		m_numEntries = numEntries;
		m_isDoubleBuffered = isDoubleBuffered;

		addMaxFileStringConstant("MemType", getType());
	}

	void addMaxFileConstant(String name, int value) {
		m_owner.addMaxFileConstant(m_memName + "_" + name, value);
	}

	void addMaxFileStringConstant(String name, String value) {
		m_owner.addMaxFileStringConstant(m_memName + "_" + name, value);
	}

	public String getMemName() { return m_memName; }
	public String getTableMemName() { return m_owner.getName() + "_" + getMemName(); }
	public String getFullName() { return m_owner.getKernel().getName() + "_" + getTableMemName(); }
	protected MaxHash<?> getOwner() { return m_owner; }
	protected DFEStructType getStructType() { return m_structType; }
	protected int getNumEntries() { return m_numEntries; }
	public boolean isDoubleBuffered() { return m_isDoubleBuffered; }

	protected abstract String getType();

	public List<Buffer> getBuffers() {
		List<Buffer> buffers = new ArrayList<Buffer>();

		if (isDoubleBuffered())
			for (Buffer b : Buffer.values())
				buffers.add(b);
		else
			buffers.add(Buffer.A);

		return buffers;
	}

	public boolean hasLMemStream() { return false; }
	public boolean hasQDRStream() { return false; }

	public DFEStruct get(DFEVar ctrl, DFEVar index) {
		return get(ctrl, index, Buffer.A);
	}

	public abstract DFEStruct get(DFEVar ctrl, DFEVar index, Buffer buffer);

	private String getNameSuffix(Buffer buffer) {
		return isDoubleBuffered() ? "_" + buffer.toString() : "";
	}

	public String getCmdStreamName(Buffer buffer) {
		return getFullName() + "_" + getType() + "ReadCmdStream" + getNameSuffix(buffer);
	}

	public String getDataStreamName(Buffer buffer) {
		return getFullName() + "_" + getType() + "ReadDataStream" + getNameSuffix(buffer);
	}

	public int getNumOccupiedBursts() {
		return 0;
	}

	protected DFEVar getReadBufferIndex(Buffer readBuffer) {
		return getOwner().constant.var(readBuffer == Buffer.B);
	}

	public abstract void connectKernelMemoryStreams(CustomManager m, KernelBlock hashBlock);
	public abstract void setupHostMemoryStreams(CustomManager m);
}
