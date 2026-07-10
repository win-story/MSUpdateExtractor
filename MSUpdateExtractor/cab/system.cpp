/* Minimal libmspack system support for MSUpdateExtractor.
 *
 * The application always passes a C++ wide-character mspack_system adapter to
 * libmspack. The default narrow stdio backend is intentionally disabled so
 * every real file operation stays on Windows wide-character APIs.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "system.h"

int mspack_version(int entity) {
  switch (entity) {
  case MSPACK_VER_MSCHMD:
  case MSPACK_VER_MSCABD:
  case MSPACK_VER_MSOABD:
    return 2;
  case MSPACK_VER_LIBRARY:
  case MSPACK_VER_SYSTEM:
  case MSPACK_VER_MSSZDDD:
  case MSPACK_VER_MSKWAJD:
    return 1;
  case MSPACK_VER_MSCABC:
  case MSPACK_VER_MSCHMC:
  case MSPACK_VER_MSLITD:
  case MSPACK_VER_MSLITC:
  case MSPACK_VER_MSHLPD:
  case MSPACK_VER_MSHLPC:
  case MSPACK_VER_MSSZDDC:
  case MSPACK_VER_MSKWAJC:
  case MSPACK_VER_MSOABC:
    return 0;
  }
  return -1;
}

int mspack_sys_selftest_internal(int offt_size) {
  return (sizeof(off_t) == offt_size) ? MSPACK_ERR_OK : MSPACK_ERR_SEEK;
}

int mspack_valid_system(struct mspack_system *sys) {
  return (sys != NULL) && (sys->open != NULL) && (sys->close != NULL) &&
    (sys->read != NULL) && (sys->write != NULL) && (sys->seek != NULL) &&
    (sys->tell != NULL) && (sys->message != NULL) && (sys->alloc != NULL) &&
    (sys->free != NULL) && (sys->copy != NULL) && (sys->null_ptr == NULL);
}

int mspack_sys_filelen(struct mspack_system *system,
                       struct mspack_file *file, off_t *length)
{
  off_t current;

  if (!system || !file || !length) return MSPACK_ERR_OPEN;

  current = system->tell(file);

  if (system->seek(file, (off_t) 0, MSPACK_SYS_SEEK_END)) {
    return MSPACK_ERR_SEEK;
  }

  *length = system->tell(file);

  if (system->seek(file, current, MSPACK_SYS_SEEK_START)) {
    return MSPACK_ERR_SEEK;
  }

  return MSPACK_ERR_OK;
}

struct mspack_system *mspack_default_system = NULL;
