#define ATTR_POS   0
#define ATTR_COLOR   1
#define ATTR_CUBEPOS 2
#define ATTR_ROT     3


#define TEXUNIT_TEMP 0
#define TEXUNIT_COLOR 1
#define TEXUNIT_AREATEX 2
#define TEXUNIT_SEARCHTEX 3
#define TEXUNIT_EDGES 4
#define TEXUNIT_BLEND 5


#ifdef __cplusplus

struct Globals

#else  // __cplusplus

layout(binding = 0, std140) uniform Globals

#endif  // __cplusplus
{
	vec4 screenSize;
	mat4 viewProj;
};
