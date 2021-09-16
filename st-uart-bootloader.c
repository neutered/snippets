#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/uio.h>

#define RESP_ACK 0x79
#define RESP_NACK 0x1f

static void hexdump(const char* tag, int ascii, const void* p, size_t nb) {
  const uint8_t* bs = (const uint8_t*)p;
  for (unsigned i = 0; i < nb; i += 16) {
    fprintf(stderr, "%s:%03x:", tag, i);
    unsigned j;
    for (j = 0; i + j < nb && j < 16; j++)
      fprintf(stderr, " %02x", bs[i + j]);
    if (ascii) {
      for (/**/; j < 16; j++)
        fputs("   ", stderr);
      fputs("  ", stderr);
      for (j = 0; i + j < nb && j < 16; j++)
        fprintf(stderr, "%c", isprint(bs[i + j]) ? bs[i + j] : '.');
    }
    fputc('\n', stderr);
  }
}

static ssize_t cmd_issue(int fd, uint8_t cmd, uint8_t* bs, size_t nbs) {
  struct timeval to;
  memset(&to, 0, sizeof(to));
  to.tv_sec = 8;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  int n = select(fd + 1, NULL, &fds, NULL, &to);
  if (n < 0)
    fprintf(stderr, "%s:%d: select:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  fprintf(stderr, "%s:%d: to:%d.%06u\n",
          __func__, __LINE__,
          to.tv_sec, to.tv_usec);
  assert(n == 1);
  assert(FD_ISSET(fd, &fds));

  /* the read commands are all one byte but all but the switch are
   * followed w/ a checksum byte.
   */
  uint8_t out[2];
  size_t nout;
  out[0] = cmd;
  if (cmd == 0x7f) {
    nout = 1;
  } else {
    nout = 2;
    out[1] = ~cmd;
  }
  ssize_t io = write(fd, out, nout);
  if (io != nout)
    fprintf(stderr, "%s:%d: write(%02x:%zu):%d:%s\n",
            __func__, __LINE__,
            cmd, nout,
            errno, strerror(errno));
  assert(io == nout);

  n = select(fd + 1, &fds, NULL, NULL, &to);
  if (n < 0)
    fprintf(stderr, "%s:%d: select:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  fprintf(stderr, "%s:%d: to:%d.%06u\n",
          __func__, __LINE__,
          to.tv_sec, to.tv_usec);
  assert(n == 1);
  assert(FD_ISSET(fd, &fds));

  /* reply */
  io = read(fd, bs, nbs);
  if (io < 0)
    fprintf(stderr, "%s:%d: read:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  assert(io > 0);
  hexdump("ack", 0, bs, io);
  return io;
}

static int param_issue(int fd, int prependn,
                       const uint8_t* param, uint8_t nparam) {
  struct timeval to;
  memset(&to, 0, sizeof(to));
  to.tv_sec = 8;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  int n = select(fd + 1, NULL, &fds, NULL, &to);
  if (n < 0)
    fprintf(stderr, "%s:%d: select:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  fprintf(stderr, "%s:%d: to:%d.%06u\n",
          __func__, __LINE__,
          to.tv_sec, to.tv_usec);
  assert(n == 1);
  assert(FD_ISSET(fd, &fds));

  /* the doc mumbles about xor(param bytes) but then says
   * for size==1 data that it's the complement. i'm not sure
   * how both can be true.
   */
  uint8_t temp = prependn ? (nparam - 1) : 0;
  uint8_t xsum;
  assert(nparam > 0);
  if (nparam == 1) {
    xsum = ~param[0];
  } else {
    xsum = temp;
    for (int i = 0; i < nparam; i++)
      xsum ^= param[i];
  }

  struct iovec iovs[3];
  int i = 0;
  if (prependn) {
    iovs[i].iov_base = &temp;
    iovs[i++].iov_len = 1;
  }
  iovs[i].iov_base = (void*)param;
  iovs[i++].iov_len = nparam;
  iovs[i].iov_base = &xsum;
  iovs[i++].iov_len = 1;
  ssize_t io = writev(fd, iovs, i);
  if (io != prependn + nparam + 1)
    fprintf(stderr, "%s:%d: write:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  assert(io == prependn + nparam + 1);

  /* wait for ack/nack */
  n = select(fd + 1, &fds, NULL, NULL, &to);
  if (n < 0)
    fprintf(stderr, "%s:%d: select:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  fprintf(stderr, "%s:%d: to:%d.%06u\n",
          __func__, __LINE__,
          to.tv_sec, to.tv_usec);
  assert(n == 1);
  assert(FD_ISSET(fd, &fds));

  uint8_t ack;
  io = read(fd, &ack, 1);
  if (io < 0)
    fprintf(stderr, "%s:%d: read:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  assert(io > 0);
  fprintf(stderr, "%s:%d: ack:%02x\n", __func__, __LINE__, ack);
  return ack == RESP_ACK;
}

static ssize_t read_n(int fd,
                      size_t o, size_t end,
                      uint8_t* bs, size_t nbs) {
  while (o < end) {
    struct timeval to;
    memset(&to, 0, sizeof(to));
    to.tv_sec = 8;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int n = select(fd + 1, &fds, NULL, NULL, &to);
    if (n < 0)
      fprintf(stderr, "%s:%d: select:%d:%s\n",
              __func__, __LINE__,
              errno, strerror(errno));
    fprintf(stderr, "%s:%d: to:%d.%06u\n",
            __func__, __LINE__,
            to.tv_sec, to.tv_usec);
    assert(n == 1);
    assert(FD_ISSET(fd, &fds));

    n = read(fd, bs + o, nbs - o);
    assert(n > 0);
    o += n;
  }
  return o;
}

static void cmd_get(int fd, uint8_t* bs, size_t nbs) {
  /* get command */
  ssize_t nb = cmd_issue(fd, 0x00, bs, nbs);
  assert(bs[0] == RESP_ACK);

  /* data should look like:
   *   ack (n-1) version cmds* ack|nack
   * where n==folling bytes up to ack|nack
   */
  assert(nb > 1);
  if (nb < 1 + 1 + bs[1] + 1 + 1)
    nb = read_n(fd, nb, 1 + 1 + bs[1] + 1 + 1, bs, nbs);
  hexdump("get", 0, bs, nb);
}

static void cmd_get_version_info(int fd, uint8_t* bs, size_t nbs) {
  /* get version info command */
  ssize_t nb = cmd_issue(fd, 0x01, bs, nbs);
  assert(bs[0] == RESP_ACK);

  /* data should look like:
   *   ack version option1 option2 ack|nack
   */
  assert(nb > 1);
  if (nb < 5)
    nb = read_n(fd, nb, 5, bs, nbs);
  hexdump("get_version_info", 0, bs, nb);
}

static void cmd_get_id(int fd, uint8_t* bs, size_t nbs) {
  /* get id command == debug id code */
  ssize_t nb = cmd_issue(fd, 0x02, bs, nbs);
  assert(bs[0] == RESP_ACK);

  /* data should look like:
   *   ack (n-1) id* ack|nack
   */
  assert(nb > 1);
  if (nb < 1 + 1 + bs[1] + 1 + 1)
    nb = read_n(fd, nb, 1 + 1 + bs[1] + 1 + 1, bs, nbs);
  hexdump("get_id", 0, bs, nb);
}

static void cmd_read_unprotect(int fd, uint8_t* bs, size_t nbs) {
  ssize_t nb = cmd_issue(fd, 0x92, bs, nbs);
  assert(bs[0] == RESP_ACK);

  /* data should look like:
   *   ack <erasing time> ack|nack
   */
  assert(nb >= 1);
  while (nb < 1 + 1)
    nb = read_n(fd, nb, 1 + 1, bs, nbs);
  hexdump("read_unprotect", 0, bs, nb);
}

static void cmd_read_mem(int fd,
                         uint32_t addr, uint32_t n,
                         uint8_t* bs, size_t nbs) {
  uint8_t temp[4];
  assert(nbs >= n);
  size_t nb = cmd_issue(fd, 0x11, temp, sizeof(temp));
  assert(temp[0] == RESP_ACK);

  /* address in big-endian */
  temp[0] = (addr >> 24) & 0xff;
  temp[1] = (addr >> 16) & 0xff;
  temp[2] = (addr >> 8) & 0xff;
  temp[3] = (addr >> 0) & 0xff;
  int issue = param_issue(fd, 0, temp, 4);
  assert(issue);

  /* send n-1 to receive n */
  assert(n > 0);
  assert(n <= 0x100);
  temp[0] = n - 1;
  issue = param_issue(fd, 0, temp, 1);
  assert(issue);

  nb = read_n(fd, 0, n, bs, nbs);
  hexdump("read_mem", 0, bs, nb);
}

static void cmd_write_mem(int fd,
                          uint32_t addr, uint32_t n,
                          const uint8_t* bs) {
  assert(n > 0);
  assert(n <= 0x100);
  assert((n & 3) == 0);

  uint8_t temp[4];
  size_t nb = cmd_issue(fd, 0x31, temp, sizeof(temp));
  assert(temp[0] == RESP_ACK);

  /* address in big-endian */
  temp[0] = (addr >> 24) & 0xff;
  temp[1] = (addr >> 16) & 0xff;
  temp[2] = (addr >> 8) & 0xff;
  temp[3] = (addr >> 0) & 0xff;
  int issue = param_issue(fd, 0, temp, 4);
  assert(issue);

  issue = param_issue(fd, 1, bs, n);
  assert(issue);
}

int main(int argc, char** argv) {
  assert(argc > 1);
  int fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0)
    fprintf(stderr, "%s:%d: open(%s):%d:%s\n",
            __func__, __LINE__,
            argv[1],
            errno, strerror(errno));
  assert(fd > 0);

  /* set mostly raw mode */
  struct termios tty;
  int err;
  memset(&tty, 0, sizeof(tty));
  tty.c_iflag = IGNBRK;
  tty.c_cflag = CS8 | CREAD | CLOCAL | PARENB;

  /* min stlink update speed is 1200bps */
  speed_t baud = B9600;
  if ((err = cfsetspeed(&tty, baud)) < 0)
    fprintf(stderr, "%s:%d: cfsetspeed:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  assert(err == 0);

  if ((err = tcsetattr(fd, TCSANOW, &tty)) < 0)
    fprintf(stderr, "%s:%d: tcsetattr:%d:%s\n",
            __func__, __LINE__,
            errno, strerror(errno));
  assert(err == 0);

  uint8_t buf[256];
  ssize_t nb;

  /* bootloader to memory boot mode.
   * if it's already switched then it replies w/ a nack
   * but is going to go to command processing.
   */
  nb = cmd_issue(fd, 0x7f, buf, sizeof(buf));
  assert(nb == 1);
  assert(buf[0] == RESP_ACK || buf[0] == RESP_NACK);

  cmd_get(fd, buf, sizeof(buf));
  cmd_get_id(fd, buf, sizeof(buf));
  cmd_get_version_info(fd, buf, sizeof(buf));
  #if 0
  /* i think the doc says that these always read as zero?
   * - option bytes should be complements
   * - only one unprotect setting
   */
  if ((buf[2] != ~buf[3]) || (buf[2] != 0xa5))
    cmd_read_unprotect(fd, buf, sizeof(buf));
  #endif
cmd_read_mem(fd, 0x20004ff0, 0x10, buf, sizeof(buf));
for (int i = 0; i < 0x10; i++)
  buf[i] = ~buf[i];
cmd_write_mem(fd, 0x20004ff0, 0x10, buf);
cmd_read_mem(fd, 0x20004ff0, 0x10, buf, sizeof(buf));

  close(fd);

  return 0;
}
