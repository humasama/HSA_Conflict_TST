#ifndef _HSA_TYPES_H_
#define _HSA_TYPES_H_

#define GRID_X 16
#define WORKGROUP_X 16

typedef uint32_t BrigCodeOffset32_t;

typedef uint32_t BrigDataOffset32_t;

typedef uint16_t BrigKinds16_t;

typedef uint8_t BrigLinkage8_t;

typedef uint8_t BrigExecutableModifier8_t;

typedef BrigDataOffset32_t BrigDataOffsetString32_t;

enum BrigKinds {
	BRIG_KIND_NONE = 0x0000,
	BRIG_KIND_DIRECTIVE_BEGIN = 0x1000,
	BRIG_KIND_DIRECTIVE_KERNEL = 0x1008,
};

typedef struct BrigBase BrigBase;
struct BrigBase {
	    uint16_t byteCount;
		    BrigKinds16_t kind;
};

typedef struct BrigExecutableModifier BrigExecutableModifier;
struct BrigExecutableModifier {
	    BrigExecutableModifier8_t allBits;
};

typedef struct BrigDirectiveExecutable BrigDirectiveExecutable;
struct BrigDirectiveExecutable {
	uint16_t byteCount;
	BrigKinds16_t kind;
	BrigDataOffsetString32_t name;
	uint16_t outArgCount;
	uint16_t inArgCount;
	BrigCodeOffset32_t firstInArg;
	BrigCodeOffset32_t firstCodeBlockEntry;
	BrigCodeOffset32_t nextModuleEntry;
	uint32_t codeBlockEntryCount;
	BrigExecutableModifier modifier;
	BrigLinkage8_t linkage;
	uint16_t reserved;
};

typedef struct BrigData BrigData;
struct BrigData {
	uint32_t byteCount;
	uint8_t bytes[1];
};
#endif
