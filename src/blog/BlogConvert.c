#include <sys/mman.h>
#include <yajl/yajl_gen.h>
#include "Blog.h"

#define STR_LEN(x) (x), (sizeof(x)-1)
#define CONVERTER(name) blog_types_##name, blog_convert_##name

typedef int (*BlogTypeCheck)(strarg_t const type);
typedef int (*BlogConverter)(
	uv_file const html,
	yajl_gen const json,
	char const *const buf,
	size_t const size,
	char const *const type);

// TODO
int blog_types_markdown(strarg_t const type);
int blog_convert_markdown(
	uv_file const html,
	yajl_gen const json,
	char const *const buf,
	size_t const size,
	char const *const type);

int blog_types_plaintext(strarg_t const type);
int blog_convert_plaintext(
	uv_file const html,
	yajl_gen const json,
	char const *const buf,
	size_t const size,
	char const *const type);

static int convert(BlogRef const blog,
                   SLNSessionRef const session,
                   char const *const htmlpath,
                   SLNSubmissionRef *const outmeta,
                   strarg_t const URI,
                   SLNFileInfo const *const src,
                   BlogTypeCheck const types,
                   BlogConverter const converter)
{
	int rc = types(src->type);
	if(rc < 0) return UV_EINVAL;

	str_t *tmp = NULL;
	uv_file html = -1;
	uv_file file = -1;
	char const *buf = NULL;
	SLNSubmissionRef meta = NULL;
	yajl_gen json = NULL;

	tmp = SLNRepoCopyTempPath(blog->repo);
	if(!tmp) rc = UV_ENOMEM;
	if(rc < 0) goto cleanup;

	rc = async_fs_open_mkdirp(tmp, O_CREAT | O_EXCL | O_WRONLY, 0400);
	if(rc < 0) goto cleanup;
	html = rc;

	rc = async_fs_open(src->path, O_RDONLY, 0000);
	if(rc < 0) goto cleanup;
	file = rc;

	// We use size+1 to get nul-termination. Kind of a hack.
	buf = mmap(NULL, src->size+1, PROT_READ, MAP_SHARED, file, 0);
	if(MAP_FAILED == buf) rc = -errno;
	if(rc < 0) goto cleanup;
	if('\0' != buf[src->size]) rc = UV_EIO; // Slightly paranoid.
	if(rc < 0) goto cleanup;

	async_fs_close(file); file = -1;

	char const *const metatype = "text/efs-meta+json; charset=utf-8";
	meta = SLNSubmissionCreate(session, metatype);
	if(!meta) rc = UV_ENOMEM;
	if(rc < 0) goto cleanup;

	SLNSubmissionWrite(meta, (byte_t const *)URI, strlen(URI));
	SLNSubmissionWrite(meta, (byte_t const *)STR_LEN("\n\n"));

	json = yajl_gen_alloc(NULL);
	if(!json) rc = UV_ENOMEM;
	if(rc < 0) goto cleanup;
	yajl_gen_config(json, yajl_gen_print_callback, (void (*)())SLNSubmissionWrite, meta);
	yajl_gen_config(json, yajl_gen_beautify, (int)true);

	yajl_gen_map_open(json);
	yajl_gen_string(json, (unsigned char const *)STR_LEN("type"));
	yajl_gen_string(json, (unsigned char const *)src->type, strlen(src->type));


	async_pool_enter(NULL);
	rc = converter(html, json, buf, src->size, src->type);
	async_pool_leave(NULL);
	if(rc < 0) goto cleanup;


	yajl_gen_map_close(json);

	rc = async_fs_fdatasync(html);
	if(rc < 0) goto cleanup;

	rc = async_fs_link_mkdirp(tmp, htmlpath);
	if(rc < 0) goto cleanup;

	rc = SLNSubmissionEnd(meta);
	if(rc < 0) goto cleanup;

	*outmeta = meta; meta = NULL;

cleanup:
	async_fs_unlink(tmp); FREE(&tmp);
	if(html >= 0) { async_fs_close(html); html = -1; }
	if(file >= 0) { async_fs_close(file); file = -1; }
	if(buf) { munmap((void *)buf, src->size+1); buf = NULL; }
	SLNSubmissionFree(&meta);
	if(json) { yajl_gen_free(json); json = NULL; }
	return rc;
}
static int generic(BlogRef const blog,
                   SLNSessionRef const session,
                   strarg_t const htmlpath,
                   SLNSubmissionRef *const outmeta,
                   strarg_t const URI,
                   SLNFileInfo const *const src)
{
	str_t *tmp = NULL;
	uv_file html = -1;
	int rc = 0;

	tmp = SLNRepoCopyTempPath(blog->repo);
	if(!tmp) rc = UV_ENOMEM;
	if(rc < 0) goto cleanup;

	rc = async_fs_open_mkdirp(tmp, O_CREAT | O_EXCL | O_WRONLY, 0400);
	if(rc < 0) goto cleanup;
	html = rc;

	preview_state const state = {
		.blog = blog,
		.session = session,
		.fileURI = URI,
	};
	rc = TemplateWriteFile(blog->preview, &preview_cbs, &state, html);
	if(rc < 0) goto cleanup;

	rc = async_fs_fdatasync(html);
	if(rc < 0) goto cleanup;

	rc = async_fs_link_mkdirp(tmp, htmlpath);
	if(rc < 0) goto cleanup;

	*outmeta = NULL;

cleanup:
	async_fs_unlink(tmp); FREE(&tmp);
	if(html >= 0) { async_fs_close(html); html = -1; }
	return rc;
}
int BlogConvert(BlogRef const blog,
                SLNSessionRef const session,
                strarg_t const html,
                SLNSubmissionRef *const meta,
                strarg_t const URI,
                SLNFileInfo const *const src)
{
	int rc = -1;
	rc=rc>=0?rc: convert(blog, session, html, meta, URI, src, CONVERTER(markdown));
	rc=rc>=0?rc: convert(blog, session, html, meta, URI, src, CONVERTER(plaintext));
	rc=rc>=0?rc: generic(blog, session, html, meta, URI, src);
	return rc;
}



// TODO
static str_t *preview_metadata(preview_state const *const state, strarg_t const var) {
	strarg_t unsafe = NULL;
	str_t buf[URI_MAX];
	if(0 == strcmp(var, "rawURI")) {
		str_t algo[SLN_ALGO_SIZE]; // SLN_INTERNAL_ALGO
		str_t hash[SLN_HASH_SIZE];
		SLNParseURI(state->fileURI, algo, hash);
		snprintf(buf, sizeof(buf), "/efs/file/%s/%s", algo, hash);
		unsafe = buf;
	}
	if(0 == strcmp(var, "queryURI")) {
		str_t *escaped = QSEscape(state->fileURI, strlen(state->fileURI), true);
		snprintf(buf, sizeof(buf), "/?q=%s", escaped);
		FREE(&escaped);
		unsafe = buf;
	}
	if(0 == strcmp(var, "hashURI")) {
		unsafe = state->fileURI;
	}
	if(unsafe) return htmlenc(unsafe);

	str_t value[1024 * 4];
	int rc = SLNSessionGetValueForField(state->session, value, sizeof(value), state->fileURI, var);
	if(DB_SUCCESS == rc && '\0' != value[0]) unsafe = value;

	if(!unsafe) {
		if(0 == strcmp(var, "thumbnailURI")) unsafe = "/file.png";
		if(0 == strcmp(var, "title")) unsafe = "(no title)";
		if(0 == strcmp(var, "description")) unsafe = "(no description)";
	}
	str_t *result = htmlenc(unsafe);

	return result;
}
static void preview_free(preview_state const *const state, strarg_t const var, str_t **const val) {
	FREE(val);
}
TemplateArgCBs const preview_cbs = {
	.lookup = (str_t *(*)())preview_metadata,
	.free = (void (*)())preview_free,
};
