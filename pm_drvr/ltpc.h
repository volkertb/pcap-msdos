/***   ltpc.h
 *
 *
 ***/

#define LT_GETRESULT  0x00
#define LT_WRITEMEM   0x01
#define LT_READMEM    0x02
#define LT_GETFLAGS   0x04
#define LT_SETFLAGS   0x05
#define LT_INIT       0x10
#define LT_SENDLAP    0x13
#define LT_RCVLAP     0x14

/* the flag that we care about */
#define LT_FLAG_ALLLAP 0x04

struct lt_getresult {
  BYTE command;
  BYTE mailbox;
};

struct lt_mem {
  BYTE command;
  BYTE mailbox;
  WORD addr;  /* host order */
  WORD length;  /* host order */
};

struct lt_setflags {
  BYTE command;
  BYTE mailbox;
  BYTE flags;
};

struct lt_getflags {
  BYTE command;
  BYTE mailbox;
};

struct lt_init {
  BYTE command;
  BYTE mailbox;
  BYTE hint;
};

struct lt_sendlap {
  BYTE command;
  BYTE mailbox;
  BYTE dnode;
  BYTE laptype;
  WORD length;  /* host order */
};

struct lt_rcvlap {
  BYTE command;
  BYTE dnode;
  BYTE snode;
  BYTE laptype;
  WORD length;  /* host order */
};

union lt_command {
  struct lt_getresult getresult;
  struct lt_mem mem;
  struct lt_setflags setflags;
  struct lt_getflags getflags;
  struct lt_init init;
  struct lt_sendlap sendlap;
  struct lt_rcvlap rcvlap;
};
typedef union lt_command lt_command;

