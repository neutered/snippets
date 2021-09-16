#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <png.h>
#include <zlib.h>

struct image_info {
uint8_t h, w;
uint16_t* pixels;
size_t nb_rle;
};

int main(int argc, const char **argv) {
  /* seed w/ black / white / roam-ish orange */
  std::set<uint16_t> color_set = { 0x0000, 0xffff, 0xe205 };
  std::unordered_map<const char*, image_info> images;

  png_bytep buffer = NULL;
  for (unsigned i = 1; i < argc; i++) {
    if ((strcmp(argv[i], ".") == 0) || (strcmp(argv[i], "..") == 0))
      continue;

    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_file(&image, argv[i]) != 0) {
      /* we don't really care about alpha but it makes it easier to
       * extract at the bit-wise level.
       */
      image.format = PNG_FORMAT_ARGB;
fprintf(stderr, "%s:%d: %s h:%u w:%u:%u %zu flags:%x\n", __func__, __LINE__, argv[i], image.height, image.width, PNG_IMAGE_ROW_STRIDE(image), PNG_IMAGE_SIZE(image), image.flags);
      assert(PNG_IMAGE_SIZE(image) == image.height * image.width * 4);
      void* p = realloc(buffer, PNG_IMAGE_SIZE(image));
      assert(p != NULL);
      buffer = (png_bytep)p;

      /* we allocate 565 rgb to do the post processing into rle */
      image_info info;
      info.h = image.height;
      info.w = image.width;
      info.pixels = (uint16_t*)malloc(image.height * image.width * sizeof(uint16_t));
      assert(info.pixels != NULL);

      if (png_image_finish_read(&image, NULL, buffer, 0, NULL) != 0) {
        /* assume little-endian bgra == png argb */
        const uint32_t* p = (const uint32_t*)buffer;
        uint16_t* o = info.pixels;
        for (unsigned i = 0; i < PNG_IMAGE_SIZE(image) / 4; i++) {
          uint16_t r = (p[i] >> 8) & 0xff;
          uint16_t g = (p[i] >> 16) & 0xff;
          uint16_t b = (p[i] >> 24) & 0xff;
          uint16_t c = ((((r & 0xf8) >> 3) << 11) |
                        (((g & 0xfc) >> 2) << 5) |
                        (((b & 0xf8) >> 3) << 0));
          color_set.insert(c);
          assert(color_set.size() <= 256);
          o[i] = c;
        }
        fprintf(stderr, "%s:%d: %s c:%u\n", __func__, __LINE__, argv[i], color_set.size());
        images[argv[i]] = info;
      } else {
        assert(0);
      }

      /* Calling png_image_free is optional unless the simplified API was
       * not run to completion.  In this case, if there wasn't enough
       * memory for 'buffer', we didn't complete the read, so we must
       * free the image:
       */
    } else {
      fprintf(stderr, "%s:%d: open(%s):%d:%s\n", __func__, __LINE__, argv[i], errno, strerror(errno));
    }
  }
  free(buffer);

  /* color table indexes */
  std::map<uint16_t, uint8_t> indexes;
  std::vector<uint16_t> colors;
  uint8_t index = 0;
  for (auto it = color_set.cbegin(); it != color_set.cend(); ++it) {
    fprintf(stderr, "%s:%d: %04x\n", __func__, __LINE__, *it);
    indexes[*it] = index++;
    colors.push_back(*it);
  }

  /* rle format
   * m<0x80 + n * *index++
   * 0x80 | n<0x80 + n * *index
   */
  for (auto it = images.begin(); it != images.end(); ++it) {
    struct run {
    unsigned n;
    uint16_t c;
    } run;
    std::vector<struct run> runs;
fprintf(stderr, "%s:%d: f:%s\n", __func__, __LINE__, it->first);
    struct image_info& info = it->second;
    const uint16_t* p = info.pixels;
    run.n = 1;
    run.c = p[0];
    runs.push_back(run);
    for (unsigned i = 1; i < info.h * info.w; i++) {
      struct run& last = runs.back();
      if (last.c == p[i]) {
        last.n++;
      } else {
        run.n = 1;
        run.c = p[i];
        runs.push_back(run);
      }
    }

    /* we use pixels as temp storage for the rle output since it can't
     * be larger, even w/ the 7-bit lenght.
     */
    uint8_t* rle = (uint8_t*)info.pixels;
fprintf(stderr, "%s:%d: %ux%u runs:%u\n", __func__, __LINE__, info.h, info.w, runs.size());
    for (unsigned i = 0; i < runs.size(); i++) {
fprintf(stderr, "%u:%04x ", runs[i].n, runs[i].c);
      if (runs[i].n > 1) {
        uint8_t ci = indexes[runs[i].c];
        /* assumes we can't overflow from the signed/unsigned */
        for (int n = runs[i].n; n > 0; n -= 0x7f) {
          *rle++ = 0x80 | (n > 0x80 ? 0x7f : n);
          *rle++ = ci;
        }
      } else {
        rle[0] = 1;
        rle[1] = indexes[runs[i].c];
        unsigned j = 2;
fprintf(stderr, "\n%s:%d: %u %u:%u %x\n", __func__, __LINE__, rle - (uint8_t*)info.pixels, i, j, runs[i].c);
        while (i + 1 < runs.size() && runs[i + 1].n == 1) {
fprintf(stderr, "%u:%04x\n", runs[i+1].n, runs[i+1].c);
          rle[0]++;
          rle[j++] = indexes[runs[++i].c];
// fprintf(stderr, "%s:%d: %u %u:%u %x\n", __func__, __LINE__, rle - (uint8_t*)info.pixels, i, j, runs[i].c);
        }
fprintf(stderr, "%s:%d: %u %u:%u\n", __func__, __LINE__, rle - (uint8_t*)info.pixels, rle[0], j);
        rle = rle + j;
      }
    }

    info.nb_rle = rle - (uint8_t*)info.pixels;
fprintf(stderr, "\nrle:%zu\n", info.nb_rle);
    rle = (uint8_t*)info.pixels;
    for (unsigned i = 0; i < info.nb_rle; /**/) {
      if (rle[i] & 0x80) {
// fprintf(stderr, "%s:%d: n:%02x %u:%04x\n", __func__, __LINE__, rle[i] & 0x7f, rle[i+1], colors[rle[i+1]]);
        i += 2;
      } else {
// fprintf(stderr, "%s:%d: n:%02x ", __func__, __LINE__, rle[i]);
// unsigned j;
// for (j = 0; j < rle[i]; j++)
// fprintf(stderr, "%u:%04x ", rle[i+1+j], colors[rle[i+1+j]]);
// fprintf(stderr, "\n");
        i += 1 + rle[i];
      }
    }
  }

  /* output pass */
  fprintf(stdout,
          "#ifndef ICONS_H_\n"
          "#define ICONS_H_\n"
          "#include <stdint.h>\n\n"
          "namespace icons {\n"
          "static const uint16_t colors[] = {\n");
size_t all = 0;
  {
    assert(colors.size() < 0x100);
    unsigned i = 0;
    for (auto it = colors.cbegin(); it != colors.cend(); ++it, i++) {
      /* 'named' color table indexes */
      if (*it == 0x0000)
        fprintf(stdout, "#define kCIBlack %u\n", i);
      else if (*it == 0xffff)
        fprintf(stdout, "#define kCIWhite %u\n", i);
      fprintf(stdout, "0x%04x,\n", *it);
all += 2;
    }
  }
  fprintf(stdout, "};\n\n");

  for (auto it = images.cbegin(); it != images.cend(); ++it) {
    /* c-ify identifiers, using the current icon set as a guide */
    std::string ident = it->first;
    if (std::string::size_type p = ident.find_last_of('.'))
      ident.resize(p);
    if (std::string::size_type p = ident.find_last_of('/'))
      ident = ident.substr(p + 1);
    for (std::string::size_type p = ident.find_first_of(' ');
         p != std::string::npos;
         p = ident.find_first_of(' ', p))
      ident[p] = '_';

    const struct image_info& info = it->second;
    fprintf(stdout,
            "namespace sprite_%s {\n"
            "constexpr uint8_t w = %u;\n"
            "constexpr uint8_t h = %u;\n"
            "constexpr size_t nb = %u;\n"
            "const uint8_t bs[] = {\n",
            ident.c_str(),
            info.w, info.h, info.nb_rle);

    const uint8_t* p = (uint8_t*)info.pixels;
all += info.nb_rle;
    for (unsigned i = 0; i < info.nb_rle; i += 16) {
      for (unsigned j = 0; i + j < info.nb_rle && j < 16; j++) {
        fprintf(stdout, "0x%02x,", p[i + j]);
      }
      fprintf(stdout, "\n");
    }
    fprintf(stdout,
            "};\n"
            "};  /* %s */\n\n",
            ident.c_str());

  }
fprintf(stderr, "%s:%d: size:%zu\n", __func__, __LINE__,  all);

fprintf(stdout,
        "};  /* icons */\n\n"
        "#endif /* ICONS_H_ */\n");

  return 0;
}
