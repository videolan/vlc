typedef struct snapshot_t {
  char *p_data;  /* Data area */

  int i_width;       /* In pixels */
  int i_height;      /* In pixels */
  int i_datasize;    /* In bytes */
  mtime_t date;      /* Presentation time */
} snapshot_t;
