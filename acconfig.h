#undef u_int16_t
#undef u_int32_t
#undef u_int64_t
#undef u_int8_t

@BOTTOM@

/* Prototypes for missing functions */

#ifndef HAVE_STRLCPY
	size_t	 strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCAT
	size_t   strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRSEP
	char *strsep(char **, const char *);
#endif /* HAVE_STRSEP */

#ifndef HAVE_ERR
	void     err(int, const char *, ...);
	void     warn(const char *, ...);
	void     errx(int , const char *, ...);
	void     warnx(const char *, ...);
#endif
