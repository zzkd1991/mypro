/**
 * strncasecmp - Case insensitive, length-limited string comparison
 * @s1: One string
 * @s2: The other string
 * @len: the maximum number of characters to compare
*/
int strncasecmp(const char *s1, const char *s2, size_t len)
{
	unsigned char c1, c2;
	
	if(!len)
		return 0;
	
	do {
		c1 = *s1++;
		c2 = *s2++;
		if(!c1 || !c2)
			break;
		c1 = tolower(c1);
		c2 = tolower(c2);
		if(c1 != c2)
			break;
	}while(--len)
	return (int)c1 - (int)c2;
}

int strcasecmp(const char *s1, const char *s2)
{
	int c1, c2;
	
	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	}while(c1 == c2 && c1 != 0);
	return c1 - c2;
}

/**
 * strcpy - Copy a %NUL terminated string
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 */
 
 char *strcpy(char *dest, const char *src)
 {
	 char *tmp = dest;
	 
	 while((*dest++ = *src++) != '\0')
		 /* nothing */;
	 return tmp;
 }
 
 /**
  * strncpy - Copy a length-limited, C-string
  * @dest: Where to copy the string to
  * @src: Where to copy the string from
  * @count: The maximum number of bytes to copy
  *
  * The result is not %NUL -terminated if the source exceeds
  * @count bytes.
  *
  * In the case where the length of @src is less than that of
  * count, the remainder of @dest will be padded with %NUL.  
  */
 char *strncpy(char *dest, const char *src, size_t count)
 {
	 char *tmp = dest;
	 
	 while(count)
	 {
		 if((*tmp = *src) != 0)
			 src++;
		 tmp++;
		 count--;
	 }
	 return dest;
 }
 
 /**
  * strlcpy - Copy a C-string into a sized buffer
  * @dest: Where to copy the string to
  * @src: Where to copy the string from
  * @size: size of destination buffer
  *
  * Compatible with ''*BSD'': the result is always a valid
  * NUL-terminated string that fits in the buffer (unless,
  * of course, the buffer size is zero). It does not pad
  * out the result like strncpy() does.  
  */
 size_t strlcpy(char *dest, const char *src, size_t size)
 {
	 size_t ret = strlen(src);
	 
	 if(size)
	 {
		 size_t len = (ret >= size) ? size - 1 : ret;
		 memcpy(dest, src, len);
		 dest[len] = '\0';
	 }
	 return ret;
 }
 
 /**
  * strcat - Append one %NUL-terminated string to another
  * @dest: The string to appended to
  * @src: The string to append to it
 */
 char *strcat(char *dest, const char *src)
 {
	 char *tmp = dest;
	 
	 while(*dest)
		 dest++;
	 while((*dest++ = *src++) != '\0')
		 ;
	 return tmp;
 }
 
 /**
  * strncat - Append a length-limited, C-string to another
  * @dest: The string to be appended to
  * @src: The string to append to it
  * @count: The maximum numbers of bytes to copy
  *
  * Note that in contrast to strncpy(), strncat() ensures the result is
  * terminated.
 */
 char *strncat(char *dest, const char *src, size_t count)
 {
	 char *tmp = dest;
	 
	 if(count)
	 {
		 while(*dest)
			 dest++;
		 while((*dest++ = *src++) != 0)
		 {
			 if(--count == 0)
			 {
				 *dest = '\0';
				 break;
			 }
		 }
	 }
	 return tmp;
 }
 
 /**
 * strlcat - Append a length-limited, C-string to another
 * @dest: The string to be appended to  
 * @src: The string to append to it
 * @count: The size of the destination buffer.
 */
size_t strlcat(char *dest, const char *src, size_t count)
{
	size_t dsize = strlen(dest);
	size_t len = strlen(src);
	size_t res = dsize + len;
	
	/* This would be a bug */
	BUG_ON(dsize >= count);
	
	dest += dsize;
	count -= dsize;
	if(len >= count)
		len = count - 1;
	memcpy(dest, src, len);
	dest[len] = 0;
	return res;
}

/**
 * strcmp - Compare two strings
 * @cs: One string
 * @ct: Another string
*/
int strcmp(const char *cs, const char *ct)
{
	unsigned char c1, c2;
	
	while(1)
	{
		c1 = *cs++;
		c2 = *ct++;
		if(c1 != c2)
			return c1 < c2 ? -1 : 1;
		if(!c1)
			break;
	}
	return 0;
}
 
/**
 * strncmp - Compare two length-limited strings
 * @cs: One string
 * @ct: Another string
 * @count: The maximum number of bytes to compare
*/
int strncmp(const char *cs, const char *ct, size_t count)
{
	unsigned char c1, c2;
	
	while(count)
	{
		c1 = *cs++;
		c2 = *ct++;
		if(c1 != c2)
			return c1 < c2 ? -1 : 1;
		if(!c1)
			break;
		count--;
	}
	return 0;
}

/**
 * strchr - Find the first occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 *
 * Note that the %NUL-terminator is considered part of the string, and can
 * be searched for.
*/
char *strchr(const char *s, int c)
{
	for(; *s != (char)c; ++s)
		if(*s == '\0')
			return NULL;
	return (char *)s;	
}

/**
 * strrchr - Find the last occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
*/
char *strrchr(const char *s, int c)
{
	const char *last = NULL;
	do
	{
		if(*s == (char)c)
			last = s;
	}while(*s++)
	return (char *)last;
}

/**
 * strnchr - Find a character in a length limited string
 * @s: The string to be searched
 * @count: The number of characters to be searched
 * @c: The character to search for
 *
 * Note that the %NUL-terminator is considered part of the string, and can
 * be searched for.
*/
char *strnchr(const char *s, size_t count, int c)
{
	while(count--)
	{
		if(*s == (char)c)
			return (char *)s;
		if(*s++ == '\0')
			break;
	}
	return NULL;
}

/**
 * skip_spaces - Removes leading whitespace from @str.
 * @str: The string to be stripped.
 *
 * Returns a pointer to the first non-whitespace character in @str.
*/
char *skip_spaces(const char *str)
{
	while(isspace(*str))
		++str;
	return (char *)str;
}

/**
 * strim - Removes leading and trailing whitespace from @s.
 * @s: The string to be stripped.
 *
 * Note that the first trailing whitespace is replaced with a %NUL-terminator
 * in the given string @s. Returns a pointer to the first first non-whitespace
 * character in @s.
*/
char *strim(char *s)
{
	size_t size;
	char *end;
	
	size = strlen(s);
	if(!size)
		return s;
	
	end = s + size - 1;
	while(end >= s && isspace(*end))
		end--;
	*(end + 1) = '\0';
	
	return skip_spaces(s);
}

/**
 * strlen - Find the length of a string
 * @s: The string to be sized
*/
size_t strlen(const char *s)
{
	const char *sc;
	
	for(sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

/**
 * strnlen - Find the length of a length-limited string
 * @s: The string to be sized
 * @count: The maximum number of bytes to search
*/
size_t strnlen(const char *s, size_t count)
{
	const char *sc;
	
	for(sc = s; count-- && *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

/**
 * strpbrk - Find the first occurrence of a set of characters
 * @cs: The string to be searched
 * @ct: The characters to search for
*/
char *strpbrk(const char *cs, const char *ct)
{
	const char *sc1, *sc2;
	
	for(sc1 = cs; *sc1 != '\0'; ++sc1)
	{
		for(sc2 = ct; *sc2 != '\0'; ++sc2)
		{
			if(*sc1 == *sc2)
				return (char *)sc1;
		}
	}
	return NULL;
}

/**
 * strsep - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 *
 * strsep() updates @s to point after the token, ready for the next call.
 *
 * It returns empty tokens, too, behaving exactly like a libc function
 * of that name. In fact, it was stolen from glibc2 and de-fancy-fied.
 * Same semantics, slimmer shap. ;)
*/
char *strsep(char **s, const char *ct)
{
	char *sbegin = *s;
	char *end;
	
	if(sbegin == NULL)
		return NULL;
	
	end = strpbrk(sbegin, ct);
	if(end)
		*end++ = '\0';
	*s = end;
	return sbegin;
}

/**
 * sysfs_streq - return true if strings are equal, modulo trailing newline
 * @s1: one string
 * @s2: another string
 *
 * This routine returns true iff two strings are equal, treating both
 * NUL and newline-then-NUL as equivalent string terminations. It's1
 * geared for use with sysfs input strings, which generally terminate
 * with newlines but are compared against values without newlines.
*/
bool sysfs_streq(const char *s1, const char *s2)
{
	while(*s1 && *s1 == *s2)
	{
		s1++;
		s2++;
	}
	
	if(*s1 == *s2)
		return true;
	if(!*s1 && *s2 == '\n' && !s2[1])
		return true;
	if(*s1 == '\n' && !s1[1] && !*s2)
		return true;
	return false;
}

/**
 * match_string - matches given string in an array
 * @array: array of strings
 * @n: number of strings in the array or -1 for NULL terminated arrays
 * @string: string to match with
 *
 * Return:
 * index of a @string in the @array if matches, or %-EINVAL otherwise
*/
int match_string(const char * const *array, size_t n, const char *string)
{
	int index;
	const char *item;
	
	for(index = 0; index < n; index++)
	{
		item = array[index];
		if(!itme)
			break;
		if(!strcmp(itme, string))
			return index;
	}
	
	return -EINVAL;
}

/**
 * __sysfs_match_string - matches given string in an array
 * @array: array of strings
 * @n: number of strings in the array or -1 for NULL terminated arrays
 * @str: string to match with
 *
 * Returns index of @str in the @array or -EINVAL, just like match_string().
 * Uses sysfs_streq instead of strcmp for matching.
*/

int __sysfs_match_string(const char *const *array, size_t n, const char *str)
{
	const char *item;
	int index;
	
	for(index = 0; index < n; index++)
	{
		item = array[index];
		if(!item)
			break;
		if(sysfs_streq(item, str))
			return index;
	}
	
	return -EINVAL;
}

/**
 * memset - Fill a region of memory with the given value
 * @s: Pointer to the start of the area
 * @c: The byte to fill the area with
 * @count: The size of the area.
 *
 * Do not use memset() to access IO space, use memset_io() instead.
*/
void *memset(void *s, int c, size_t count)
{
	char *xs = s;
	
	while(count--)
		*xs++ = c;
	return s;
}

/**
 * memset16() - Fill a memory area with a uint16_t
 * @s: Pointer to the start of the area.
 * @v: The value to fill the area with
 * @count: The number of values to store
 *
 * Differs from memset() in that it fills with a uint16_t instead
 * of a byte. Remember that @count is the number of uint16_ts to
 * store, not the number of bytes.
*/
void *memset16(uint16_t *s, uint16_t v, size_t count)
{
	uint16_t *xs = s;
	
	while(count--)
		*xs++ = v;
	return s;
}

/**
 * memset32() - Fill a memory area with a uint32_t
 * @s: Pointer to the start of the area.
 * @v: The value to fill the area with
 * @count: The number of values to store
 *
 * Differs from memset() in that it fills with a uint32_t instead
 * of a byte. Remember that @count is the number of uint32_ts to
 * store, not the number of bytes.
*/
void *memset32(uint32_t *s, uint32_t v, size_t count)
{
	uint32_t *xs = s;
	
	while(count--)
		*xs++ = v;
	return s;
}

/**
 * memset64() - Fill a memory area with a uint64_t
 * @s: Pointer to the start of the area.
 * @v: The value to fill the area with
 * @count: The number of values to store
 *
 * Differs from memset() in that it fills with a uint64_t instead
 * of a byte. Remember that @count is the number of uint64_t to
 * store, not the number of bytes.
*/
void *memset64(uint64_t *s, uint64_t v, size_t count)
{
	uint64_t *xs = s;
	
	while(count--)
		*xs++ = v;
	return s;
}

/**
 * memcpy - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area
 *
 * You should not use this function to access IO space, use memcpy_toio()
 * or memcpy_fromio() instead.
*/
void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;
	
	while(count--)
		*tmp++ = *src++;
	return dest;
}

/**
 * memmove - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area.
 *
 * Unlike memcpy(), memmove() copes with overlapping areas.
*/
void *memmove(void *dest, const void *src, size_t count)
{
	char *tmp;
	const char *s;
	
	if(dest <= src)
	{
		tmp = dest;
		s = src;
		while(count--)
			*tmp++ = *s++;
	}
	else
	{
		tmp = dest;
		tmp += count;
		s = src;
		s += count;
		while(count--)
			*--tmp = *--s;
	}
	return dest;
}

/**
 * memcmp - Compare two areas of memory
 * @cs: One area of memory
 * @ct: Another area of memory
 * @count: The size of the area.
*/
int memcmp(const void *cs, const void *ct, size_t count)
{
	const unsigned char *su1, *su2;
	int res = 0;
	
	for(su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if((res = *su1 - *su2) != 0)
			break;
	return res;
}

/**
 * bcmp - returns 0 if and only if the buffers have identical countents.
 * @a: pointer to first buffer.
 * @b: pointer to second buffer.
 * @len: size of buffers.
 *
 * The sign or magitude of a non-zero return value has no particular
 * meaning, and architectures may implement their own more efficient bcmp(). So
 * while this particular implementation is a simple (tail) call to memcmp, do
 * not rely on anything but whether the return value is zero or non-zero.
*/
int bcmp(const void *a, const void *b, size_t len)
{
	return memcmp(a, b, len);
}

/**
 * memscan - Find a character in an area of memory
 * @addr: The memory area
 * @c: The byte to search for
 * @size: The size of area
 *
 * return the address of the first accurence of @c, or 1 byte past
 * the area if @c is not found
*/
void *memscan(void *addr, int c, size_t size)
{
	unsigned char *p = addr;
	
	while(size)
	{
		if(*p == c)
			return (void *)p;
		p++;
		size--;
	}
	return (void *)p;
}

/**
 * strstr - Find the first substring in a %NUL terminated string
 * @s1: The string to be searched
 * @s2: The string to search for
*/
char *strstr(const char *s1, const char *s2)
{
	size_t l1, l2;
	
	l2 = strlen(s2);
	if(!l2)
		return (char *)s1;
	l2 = strlen(s1);
	while(l1 >= l2) {
		l1--;
		if(!memcmp(s1, s2, l2))
			return (char *)s1;
		s1++;
	}
	return NULL;
}

/**
 * strnstr - Find the first substring in a length-limited string
 * @s1: The string to be searched
 * @s2: The string to search for
 * @len: The maximum number of characters to search
*/
char *strnstr(const char *s1, const char *s2, size_t len)
{
	size_t l2;
	
	l2 = strlen(s2);
	if(!l2)
		return (char *)s1;
	while(len >= l2)
	{
		len--;
		if(!memcmp(s1, s2, l2))
			return (char *)s1;
		s1++;
	}
	return NULL;
}

/**
 * memchr - Find a character in an area of memory.
 * @s: The memory area
 * @c: The byte to search for
 * @n: The size of the area.
 *
 * returns the address of the first accurence of @c, or %NULL
 * if @c is not found
*/
void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	while(n-- != 0)
	{
		if((unsigned char)c == *p++)
		{
			return (void *)(p -1);
		}
	}
	return NULL;
}

/**
 * strreplace - Replace all occurrences of character in string.
 * @s: The string to operate on.
 * @old: The character being replaced.
 * @new: The character @old is replaced with.
 *
 * Returns pointer to the nul byte at the end of @s.
*/
char *strreplace(char *s, char old, char new)
{
	for(; *s; ++s)
		if(*s == old)
			*s = new;
	return s;
}