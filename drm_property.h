#ifndef __DRM_PROPERTY_H__
#define __DRM_PROPERTY_H__

struct drm_property_enum {
	uint64_t value;
	struct list_head head;
	char name[DRM_PROP_NAME_LEN];
};

struct drm_property {
	struct list_head head;
	struct drm_mode_object base;
	uint32_t flags;
	char name[DRM_PROP_NAME_LEN];
	uint32_t num_values;
	uint64_t *values;
	struct drm_device *dev;
	struct list_head enum_list;
};

struct drm_property_blob {
	struct drm_mode_object base;
	struct drm_device *dev;
	struct list_head head_global;
	struct list_head head_file;
	size_t length;
	void *data;
};

struct drm_prop_enum_list {
	int type;
	const char *name;
};

#define obj_to_property(x)	container_of(x, struct drm_property, base)
#define obj_to_blob(x) container_of(x, struct drm_property_blob, base)

static inline bool drm_property_type_is(struct drm_property *property, uint32_t type)
{
	if(property->flags & DRM_MODE_PROP_EXTENDED_TYPE)
		return (property->flags & DRM_MODE_PROP_EXTENDED_TYPE) == type;
	return property->flags & type;
}




#endif




