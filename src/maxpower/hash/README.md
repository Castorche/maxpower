MaxHash
========

Overview
---------

MaxHash is an implementation of a hash table in MaxCompiler.
The most common use case would be that of a Perfect Minimal Hash.

The hash-table can be backed by either FMEM or LMEM.
A perfect minimal hash consists of 2 tables: A value table and an Intermediate table.


Configuration Options
----------------------


* setKeyValid - set the DFEVar which states whether the key passed in in the current cycle is valid.  The key is the item you're looking up in the hash table.
* setKeyType - sets the DFEType of the key.
* setValueType - sets the DFEType of the value.  The value is the item that's stored in the hash table, which could be an address to an entry, or the entry itself.
* setJenkinsChunkWidth - sets number of bits of input key to be hashed on each clock cycle.  Standard value is 8 (8 bits processed in parallel on each cycle), but larger values are often needed to reduce latency.  Setting it to be too large may affect hashing efficiency.
* setNumBuckets - for a minimal perfect hash table, which is what we're dealing with, this is essentially the capacity of the table.
* setMaxBucketEntries - for a minimal perfect hash table, this should be 1.
* setPerfect - for a minimal perfect hash table, this should be 'true'.
* setMemType - type of memory used to store the values in the hash table.
* setHashParamMemType - type of memory used to store intermediate values required by the minimal perfect hashing algorithm that we use. This table can be smaller than the values table, which might mean that it should use a different type of memory for best performance.
* setNumIntermediateEntries - size of intermediate table, normally equal to NumBuckets, but can be smaller in order to save memory at the expense of greater compute requirements in software when the hash table is changed (re-committed).
* setValidateResults - whether the key should be stored alongside the value in the values table.  If this is set to false and we pass in a key that wasn't in the original set of keys that we put in the software hash table, the table will return (via hash.get()) a random entry and hash.isValid() will erroneously be set to true.  Thus, if you can guarantee that any entry that is requested from the hash table was in the set of keys added to the hash table in software, this can safely be set to 'false', but if you need to know for a given key whether it was in that set, set it to 'true'.  Setting it to 'true' increases memory requirements, since we need to store keys as well as values in the value table.

Instantiation Example
---------------------

Kernel Code:

```
MaxHashParameters<DFEVar> mhp = new MaxHashParameters<DFEVar>(manager, "TableName", 
		Key.getType(),
		dfeUInt(32), // Element type
		4096, // Number of Elements
		MemType.FMEM);
mhp.setJenkinsChunkWidth(32);
mhp.setMaxBucketEntries(1); // Needs to be 1 for MPH
mhp.setPerfect();
mhp.setValidateResults(true);

MaxHash<DFEVar> hash = MaxHashFactory.create(this, mhp, key, start);

DFEVar value = hash.get();
DFEVar contains = hash.containsKey();
```

Runtime Code:

Initialization:
```
maxhash_engine_state_t *engine_state = malloc(sizeof(maxhash_engine_state_t));
engine_state->maxfile = maxfile;
engine_state->engine = engine;

maxhash_table_t *table;

if (maxhash_hw_table_init(&table, "MyKernel", "HashTableName", engine_state) != MAXHASH_ERR_OK) {
	printf("Failed to initialize hash-table\n");	
}
```

Populating:
```
maxhash_put(table, key, SIZE_OF_KEY, &value, sizeof(value));

// When you want the HW to be updated
maxhash_commit(table)
```

