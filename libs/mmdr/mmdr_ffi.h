#ifndef MMDR_FFI_H
#define MMDR_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render a mermaid diagram to SVG.
 *
 * @param input     Null-terminated UTF-8 mermaid diagram text
 * @param output    Pointer to receive the output string (SVG on success, error message on failure)
 * @return          0 on success, -1 on error
 *
 * The caller must free *output with mmdr_free() after use.
 */
int mmdr_render_svg(const char *input, char **output);

/**
 * Free a string allocated by mmdr_render_svg.
 */
void mmdr_free(char *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MMDR_FFI_H */
