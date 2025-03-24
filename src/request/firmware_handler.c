#include "firmware_handler.h"

#include <definitions.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../firmware/firmware_download.h"
#include "request_common.h"

void firmware_request_handler(nxt_unit_request_info_t *req_info, int rc) {
  char *p;
  nxt_unit_buf_t *buf;
  struct stat file_info;
  FILE *fptr;
  size_t io_blocks;

  rc = response_init(req_info, rc, 200, OCTET_STREAM);
  if (rc == 1) {
    goto fail;
  }

  rc = download_firmware_github(&fptr);
  if (rc) {
    nxt_unit_req_error(req_info, "Failed to get latest firmware from GitHub");
    goto fail;
  }

  if (fstat(fileno(fptr), &file_info) != 0) {
    perror("[firmware_request_handler] Failed to stat firmware file. Error");
    goto fail;
  }

  /* file_info.st_blocks is the number of 512B blocks allocated on Linux.
   * file_info.st_blksize is the preferred blocksize for file system I/O, likey
   * larger that 512B.
   */
  if (file_info.st_blksize > 512) {
    io_blocks = file_info.st_blocks / (file_info.st_blksize / 512);
  } else if (file_info.st_blksize < 512) {
    io_blocks = file_info.st_blocks * (512 / file_info.st_blksize);
  } else {
    io_blocks = file_info.st_blocks;
  }

  PRINTDBG("Firmware file size: %ld B", file_info.st_size);
  PRINTDBG("I/O block size: %ld B", file_info.st_blksize);
  PRINTDBG("Blocks allocated: %ld", io_blocks);

  PRINTDBG("Sending firmware...");
  for (size_t i = 0; i < io_blocks; i++) {
    buf = nxt_unit_response_buf_alloc(req_info, (file_info.st_blksize));
    if (buf == NULL) {
      rc = NXT_UNIT_ERROR;
      PRINTERR("Failed to allocate response buffer");
      goto fail;
    }
    p = buf->free;

    if (fread(p, file_info.st_blksize, 1, fptr) == 1) {
      buf->free = (p + file_info.st_blksize);
    } else if (ferror(fptr)) {
      perror("Failed to read firmware file. Error");
      goto fail;
    } else {
      /* Reached EOF, we're on the last buffer. The remaining file data is
       * smaller than the allocated memory and we don't want to send extra data.
       * To get the actual size of data in this buffer we have to do some math:
       * The size of total bytes alocated - actual file size bytes gives us the
       * size of unused memory in bytes. We subtract that from the size of the
       * current buffer, file_info.st_blksize.
       */
      buf->free = (p + (file_info.st_blksize - ((file_info.st_blocks * 512) - file_info.st_size)));
    }

    rc = nxt_unit_buf_send(buf);
    if (rc) {
      PRINTERR("Failed to send buffer");
      goto fail;
    }
  }
  PRINTDBG("Done");

fail:
  if (fclose(fptr)) {
    perror("Failed to close firmware file. Error");
  }
  nxt_unit_request_done(req_info, rc);
}
