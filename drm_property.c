
struct drm_property *drm_property_create(struct drm_device *dev, u32 flags, const char *name, int num_values)
{
	struct drm_property *property = NULL;
	int ret;
	
	if(WARN_ON(!drm_property_flags_valid(flags)))
		return NULL;
	
	if(WARN_ON(strlen(name) >= DRM_PROP_NAME_LEN))
		return NULL;
	
	property = kzalloc(sizeof(struct drm_property), GFP_KERNEL);
	if(!property)
		return NULL;
	
	property->dev = dev;
	
	if(num_values)
	{
		property->values = kcalloc(name_values, sizeof(uint64_t), GFP_KERNEL);
		
		if(!property->values)
			goto fail;
	}
	
	ret = drm_mode_object_add(dev, &property->base, DRM_MODE_OBJECT_PROPERTY);
	if(ret)
		goto fail;
	
	property->flags = flags;
	property->num_values = num_values;
	INIT_LIST_HEAD(&property->enum_list);
	
	strncpy(property->name, name, DRM_PROP_NAME_LEN);
	property->name[DRM_PROP_NAME_LEN -1] = '\0';
	
	list_add_tail(&property->head, &dev->mode_config.property_list);
	
	return property;
fail:
	kfree(property->values);
	kfree(property);
	return NULL;
}

struct drm_property *drm_property_create_enum(struct drm_device *dev,
						u32 flags, const char *name,
						const struct drm_prop_enum_list *props,
						int num_values)
{
	struct drm_property *property;
	int i, ret;
	
	flags |= DRM_MODE_PROP_ENUM;
	
	property = drm_property_create(dev, flags, name, num_values);
	if(!property)
		return NULL;
	
	for(i = 0; i < num_values; i++)
	{
		ret = drm_property_add_enum(property,
							props[i].type,
							props[i].name);
		if(ret)
		{
			drm_property_destroy(dev, property);
			return NULL;
		}
	}
	
	return property;
}

struct drm_property *drm_property_create_bitmask(struct drm_device *dev,
						u32 flags, const char *name,
						const struct drm_prop_enum_list *props,
						int num_props,
						uint64_t supported_bits)
{
	struct drm_property *property;
	int i, ret;
	int num_values = hweight64(supported_bits);
	
	flags |= DRM_MODE_PROP_BITMASK;
	
	property = drm_property_create(dev, flags, name, num_values);
	if(!property)
		return NULL;
	for(i = 0; i < num_props; i++)
	{
		if(!(supported_bits & (1ULL << props[i].type)))
			continue;
		
		ret = drm_property_add_enum(property,
							props[i].type,
							props[i].name);
		if(ret)
		{
			drm_property_destroy(dev, property);
			return NULL;
		}
	}
	
	return property;
}

static struct drm_property *property_create_range(struct drm_device *dev,
									u32 flags, const char *name,
									uint64_t min, uint64_t max)
{
	struct drm_property *property;
	
	property = drm_property_create(dev, flags, name, 2);
	if(!property)
		return NULL;
	
	property->values[0] = min;
	property->vlaues[1] = max;
	
	return property;
}

struct drm_property *drm_property_create_range(struct drm_device *dev,
									u32 flags, const char *name,
									uint64_t min, uint64_t max)
{
	return property_create_range(dev, DRM_MODE_PROP_RANGE | flags,
				name, min, max);
}

struct drm_property *drm_property_create_object(struct drm_device *dev,
						u32 flags, const char *name,
						uint32_t type)
{
	struct drm_property *property;
	
	flags |= DRM_MODE_PROP_OBJECT;
	
	if(WARN_ON(!(flags & DRM_MODE_PROP_ATOMIC)))
		return NULL;
	
	property = drm_property_create(dev, flags, name, 1);
	if(!property)
		return NULL;
	
	property->values[0] = type;
	
	return property;
}						

struct drm_property *drm_property_create_bool(struct drm_device *dev,
								u32 flags, const char *name)
{
	return drm_property_create_range(dev, flags, name, 0, 1);
}

int drm_property_add_enum(struct drm_property *property,
					uint64_t value, const char *name)
{
	struct drm_property_enum *prop_enum;
	int index = 0;
	
	if(WARN_ON(strlen(name) >= DRM_PROP_NAME_LEN))
		return -EINVAL;
	
	if(WARN_ON(!drm_property_type_is(propty, DRM_MODE_PROP_ENUM) &&
		!drm_property_type_is(property, DRM_MODE_PROP_BITMASK)))
			return -EINVAL;
			
	if(WARN_ON(drm_property_type_is(property, DRM_MODE_PROP_BITMASK) &&
				value > 63))
			return -EINVAL;
			
	list_for_each_entry(prop_enum, &property->enum_list, head)
	{
		if(WARN_ON(prop_enum->value == value))
			return -EINVAL;
		index++;
	}
	
	if(WARN_ON(index >= property->num_values))
		return -EINVAL;
	
	prop_enum = kzalloc(sizeof(struct drm_property_enum), GFP_KERNEL);
	if(!prop_enum)
		return -ENOMEM;
	
	strncpy(prop_enum->name, name, DRM_PROP_NAME_LEN);
	prop_enum->name[DRM_PROP_NAME_LEN - 1] = '\0';
	prop_enum->value = value;
	
	property->values[index] = values;
	list_add_tail(&prop_enum->head, &property->enum_list);
	return 0;
}

void drm_property_destroy(struct drm_device *dev, struct drm_property *property)
{
	struct drm_property_enum *prop_enum, *pt;
	
	list_for_each_entry_safe(prop_enum, pt, &property->enum_list, head)
	{
		list_del(&prop_enum->head);
		kfree(prop_enum);
	}
	
	if(property->enum_values)
		kfree(property->values);
	drm_mode_object_unregister(dev, &property->base);
	list_del(&property->head);
	kfree(property);
}

int drm_mode_getproperty_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_property *out_resp = data;
	struct drm_property *property;
	int enum_count = 0;
	int value_count = 0;
	int i, copied;
	struct drm_property_enum *prop_enum;
	struct drm_mode_property_enum __user *enum_ptr;
	uint64_t __user *values_ptr;
	
	if(!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EIPNOTSUPP;
	
	property = drm_property_find(dev, file_priv, out_resp->prop_id);
	if(!property)
		return -ENOENT;
	
	strncpy(out_resp->name, property->name, DRM_PROP_NAME_LEN);
	out_resp->name[DRM_PROP_NAME_LEN - 1] = 0;
	out_resp->flags = property->flags;
	
	value_count = property->num_values;
	values_ptr = u64_to_user_ptr(out_resp->values_ptr);
	
	for(i = 0; i < value_count; i++)
	{
		if(i < out_resp->count_values &&
			put_user(property->values[i], values_ptr + i))
		{
			return -EFAULT;
		}
	}
	out_resp->count_values = value_count;
	
	copied = 0;
	enum_ptr = u64_to_user_ptr(out_resp->enum_blob_ptr);
	
	
	if(drm_property_type_is(property, DRM_MODE_PROP_ENUM) ||
		drm_property_type_is(property, DRM_MODE_PROP_BITMASK))
	{
		list_for_each_entry(prop_enum, &property->enum_list, head)
		{
			enum_count++;
			if(out_resp->count_enum_blobs < enum_count)
				continue;
			
			if(copy_to_user(&enum_ptr[copied].value,
					&prop_enum->value, sizeof(uint64_t)))
				return -EFAULT;
				
			if(copy_to_user(&enum_ptr[copied].name, &prop_enum->name, DRM_PROP_NAME_LEN))
				return -EFAULT;
			copied++;
		}
		out_resp->count_enum_blobs = enum_count;
	}
	
	if(drm_property_type_is(property, DRM_MODE_PROP_BLOB))
		out_resp->count_enum_blobs = 0;
	
	return 0;
}

static void drm_property_free_blob(struct kref *kref)
{
	struct drm_property_blob *blob = 
			container_of(kref, struct drm_property_blob, base.refcount);
			
	mutex_lock(&blob->dev->mode_config.blob_lock);
	list_del(&blob->head_global);
	mutex_unlock(&blob->dev->mode_config.blob_lock);
	
	drm_mode_object_unregister(blob->dev, &blob->base);
	
	kvfree(blob);
}

struct drm_property_blob *
drm_property_create_blob(struct drm_device *dev, size_t length,
		const void *data)
{
	struct drm_property_blob *blob;
	int ret;
	
	if(!length || length > UNLONG_MAX - sizeof(struct drm_property_blob))
		return ERR_PTR(-EINVAL);
	
	blob = kvzalloc(sizeof(struct drm_property_blob)+length, GFP_KERNEL);
	if(!blob)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&blob->head_file);
	blob->data = (void *)blob + sizeof(*blob);
	blob->length = length;
	blob->dev = dev;
	
	if(data)
		memcpy(blob->data, data, length);

	ret = __drm_mode_object_add(dev, &blob->base, DRM_MODE_OBJECT_BLOB,
					true, drm_property_free_blob);
	if(ret)
	{
		kvfree(blob);
		return ERR_PTR(-EINVAL);
	}
	
	
	mutex_lock(&dev->mode_config.blob_lock);
	list_add_tail(&blob->head_global,
					&dev->mode_config.property_blob_list);
	mutex_unlock(&dev->mode_config.blob_lock);

	return blob;
}

void drm_property_blob_put(struct drm_property_blob *blob)
{
	if(!blob)
		return;
	
	drm_mode_object_put(&blob->base);
}

void drm_property_destroy_user_blobs(struct drm_device *dev,
					struct drm_file *file_priv)
{
	struct drm_property_blob *blob, *bt;
	
	list_for_each_entry_safe(blob, bt, &file_priv->blobs, head_file)
	{
		list_del_init(&blob->head_file);
		drm_property_blob_put(blob);
	}
}

struct drm_property_blob *drm_property_blob_get(struct drm_property_blob *blob)
{
	drm_mode_object_get(&blob->base);
	return blob;
}

struct drm_property_blob *drm_property_lookup_blob(struct drm_device *dev,
										uint32_t id)
{
	struct drm_mode_object *obj;
	struct drm_property_blob *blob = NULL;
	
	obj = __drm_mode_object_find(dev, NULL, id, DRM_MODE_OBJECT_BLOB);
	if(obj)
		blob = obj_to_blob(obj);
	
	return blob;
}

int drm_property_replace_global_blob(struct drm_device *dev,
								struct drm_property_blob **replace,
								size_t length,
								const void *data,
								struct drm_mode_object *obj_holds_id,
								struct drm_property *prop_holds_id)
{
	struct drm_property_blob *new_blob = NULL;
	struct drm_property_blob *old_blob = NULL;
	int ret;
	
	WARN_ON(replace == NULL);
	
	old_blob = *replace;
	
	if(length && data)
	{
		new_blob = drm_property_create_blob(dev, length, data);
		if(IS_ERR(new_blob))
			return PTR_ERR(new_blob);
	}
	
	if(obj_holds_id)
	{
		ret = drm_object_property_set_value(obj_holds_id,
										prop_holds_id,
										new_blob ?
											new_blob->base.id : 0);
		if(ret != 0)
			goto err_created;
	}
	
	drm_property_blob_put(old_blob);
	*replcae = new_blob;
	
	return 0;
	
err_created:
	drm_property_blob_put(new_blob);
	return ret;
}

bool drm_property_replace_blob(struct drm_property_blob **blob,
						struct drm_property_blob *new_blob)
{
	struct drm_property_blob *old_blob = *blob;
	
	if(old_blob == new_blob)
		return false;
	
	drm_property_blob_put(old_blob);
	if(new_blob)
		drm_property_blob_get(new_blob);
	*blob = new_blob;
	return true;
}

int drm_mode_getblob_ioctl(struct drm_device *dev,
						void *data, struct drm_file **file_priv)
{
	struct drm_mode_get_blob *out_resp = data;
	
	
	
	
}