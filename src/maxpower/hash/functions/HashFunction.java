package maxpower.hash.functions;

import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFEType;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFETypeFactory;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFEVar;

public interface HashFunction {

	public DFEVar hash(DFEVar key);
	public DFEVar hash(DFEVar key, DFEVar param);
	public DFEType getType();

	public static final int OCTET_SIZE = 8;

	public class TrivialHash implements HashFunction {

		private final int m_width;

		public TrivialHash(int width) {
			m_width = width;
		}

		@Override
		public DFEVar hash(DFEVar key) {
			return key.cast(getType());
		}

		@Override
		public DFEVar hash(DFEVar key, DFEVar param) {
			return hash(key);
		}

		@Override
		public DFEType getType() {
			return DFETypeFactory.dfeUInt(m_width);
		}
	}
}
