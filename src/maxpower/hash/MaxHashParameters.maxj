package maxpower.hash;

import maxpower.hash.mem.MemInterface.MemType;

import com.maxeler.maxcompiler.v2.kernelcompiler.types.KernelObject;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.KernelType;
import com.maxeler.maxcompiler.v2.kernelcompiler.types.base.DFEType;
import com.maxeler.maxcompiler.v2.managers.custom.CustomManager;
import com.maxeler.maxcompiler.v2.utils.MathUtils;

public class MaxHashParameters<T extends KernelObject<T>> {
	private final CustomManager manager;
	private final String name;
	private final DFEType keyType;
	private final KernelType<T> valueType;

	private final int numValuesBuckets;
	private final MemType valuesMemType;

	private int numIntermediateBuckets = 0;
	private MemType intermediateMemType = MemType.UNDEFINED;

	private int baseAddressBursts;
	private boolean validateResults = true;

	private boolean doubleBufferingEnabled = true;
	private int maxBucketEntries = 1;
	private int jenkinsChunkWidth = 32;

	private boolean debugMode = false;

	public MaxHashParameters(
			CustomManager manager,
			String name,
			DFEType keyType,
			KernelType<T> valueType,
			int numBuckets,
			MemType memType) {

		if (keyType.getTotalBits() % 8 != 0)
			throw new MaxHashException("Invalid key type: bit width must be a " +
					"multiple of 8.  Value provided was " +
					keyType.getTotalBits() + ".");

		if (!MathUtils.isPowerOf2(numBuckets))
			throw new MaxHashException("Invalid table size: must be a power of 2.");

		this.manager = manager;
		this.name = name;
		this.keyType = keyType;
		this.valueType = valueType;
		this.numValuesBuckets = numBuckets;
		this.valuesMemType = memType;
	}

	/**
	 * Set whether or not the hash table should guarantee that no collisions
	 * will occur between any pair of keys from a given set, provided in the
	 * software support library.
	 *
	 * The size of the intermediate table is set to the same size as the
	 * values table, and the memory type is set to the same type as the
	 * values table.
	 */
	public void setPerfect() {
		setPerfect(this.numValuesBuckets, this.valuesMemType);
	}

	/**
	 * Set whether or not the hash table should guarantee that no collisions
	 * will occur between any pair of keys from a given set, provided in the
	 * software support library.
	 *
	 * The memory type is set to the same type as the values table.
	 *
	 * @param numIntermediateBuckets The size of the intermediate table (each
	 *        bucket contains a maximum of one entry)
	 */
	public void setPerfect(int numIntermediateBuckets) {
		setPerfect(numIntermediateBuckets, this.valuesMemType);
	}

	/**
	 * Set whether or not the hash table should guarantee that no collisions
	 * will occur between any pair of keys from a given set, provided in the
	 * software support library.
	 *
	 * The size of the intermediate table is set to the same size as the
	 * values table.
	 *
	 * @param intermediateMemType The type of memory used to store entries
	 */
	public void setPerfect(MemType intermediateMemType) {
		setPerfect(this.numValuesBuckets, intermediateMemType);
	}

	/**
	 * Set whether or not the hash table should guarantee that no collisions
	 * will occur between any pair of keys from a given set, provided in the
	 * software support library.
	 *
	 * @param numIntermediateBuckets The size of the intermediate table (each
	 *        bucket contains a maximum of one entry)
	 * @param intermediateMemType The type of memory used to store entries
	 */
	public void setPerfect(int numIntermediateBuckets, MemType intermediateMemType) {
		if (this.maxBucketEntries != 1)
			throw new MaxHashException("Maximum bucket entries must be set to 1 for perfect hash tables.");
		setMaxBucketEntries(1);
		this.numIntermediateBuckets = numIntermediateBuckets;
		this.intermediateMemType = intermediateMemType;
	}

	/**
	 * Set the maximum number of collisions permitted in each bucket.  The
	 * default value is 1, and cannot be changed in perfect hash tables.
	 *
	 * @param maxBucketEntries The maximum number of collisions permitted
	 *        in each bucket
	 */
	public void setMaxBucketEntries(int maxBucketEntries) {
		if (maxBucketEntries != 1 && isPerfect())
			throw new MaxHashException("Maximum bucket entries must be set to 1 for perfect hash tables.");
		this.maxBucketEntries = maxBucketEntries;
	}

	/**
	 * Set the base address of LMem-backed hash tables.  For hash tables that
	 * are not backed by LMem, this parameter is ignored.
	 */
	public void setBaseAddressBursts(int baseAddressBursts) {
		this.baseAddressBursts = baseAddressBursts;
	}

	/**
	 * Enable or disable storage of the key alongside the value in the
	 * hash table, so that results may be verified.  This is enabled by
	 * default, and for some types of hash table it cannot be disabled.
	 *
	 * @param validateResults Whether or not to enable result validation
	 */
	public void setValidateResults(boolean validateResults) {
		this.validateResults = validateResults;
	}

	/**
	 * Enable or disable double buffering, so that entries can be read
	 * concurrently with new entries being loaded.  This is enabled by
	 * default.
	 *
	 * @param validateResults Whether or not to enable result validation
	 */
	public void setDoubleBufferingEnabled(boolean doubleBufferingEnabled) {
		this.doubleBufferingEnabled = doubleBufferingEnabled;
	}

	public void setJenkinsChunkWidth(int jenkinsChunkWidth) {
		if (!MathUtils.isPowerOf2(jenkinsChunkWidth) || jenkinsChunkWidth % 8 != 0)
			throw new MaxHashException("Invalid Jenkins chunk width: must be a power of 2 and a multiple of 8.");

		this.jenkinsChunkWidth = jenkinsChunkWidth;
	}

	public void setDebugMode(boolean debugMode) {
		this.debugMode = debugMode;
	}






	/* GETTERS */

	int getNumIntermediateBuckets() {
		return numIntermediateBuckets;
	}

	int getBaseAddressBursts() {
		return baseAddressBursts;
	}

	CustomManager getManager() {
		return manager;
	}

	String getName() {
		return name;
	}

	DFEType getKeyType() {
		return keyType;
	}

	KernelType<T> getValueType() {
		return valueType;
	}

	int getJenkinsChunkWidth() {
		return jenkinsChunkWidth;
	}

	int getNumValuesBuckets() {
		return numValuesBuckets;
	}

	int getMaxBucketEntries() {
		return maxBucketEntries;
	}

	MemType getValuesMemType() {
		return valuesMemType;
	}

	MemType getIntermediateMemType() {
		return intermediateMemType;
	}

	boolean isValidateResults() {
		return validateResults;
	}

	boolean isPerfect() {
		return numIntermediateBuckets != 0 && intermediateMemType != MemType.UNDEFINED;
	}

	boolean isDoubleBufferingEnabled() {
		return doubleBufferingEnabled;
	}

	boolean isDebugMode() {
		return debugMode;
	}
}
