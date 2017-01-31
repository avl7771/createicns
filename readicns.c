// -*- Mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2017, Arjan van Leeuwen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// This is a converter for the Apple Icon Image Format (.icns) which outputs
// PNGs without changing them.
//
// It's run like this: 'readicns x.icns' and outputs a directory 'x.iconset'.
//
// This tool is similar to running 'iconutil -c iconset x.icns', except it
// doesn't change the PNG images in any way.

#include <arpa/inet.h>
#include <dirent.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

// Magic values for headers were found at
// https://en.wikipedia.org/wiki/Apple_Icon_Image_format.

enum kBufferSize { kBufferSize = 1024 };
static const char kIconsetExtension[] = ".iconset";
static const char kIcnsExtension[] = ".icns";
static const char kUnknownFormatFilename[] = "icon_data_";
static const uint32_t kMagicHeader = 'icns';

typedef struct {
  char path[MAXPATHLEN];
} Path;

struct {
  const char* icon_filename;
  uint32_t icon_type;
} static const kIconTypes[] = {
    {"icon_16x16.png", 'icp4'},
    {"icon_16x16@2x.png", 'ic11'},
    {"icon_32x32.png", 'icp5'},
    {"icon_32x32@2x.png", 'ic12'},
    {"icon_64x64.png", 'icp6'},
    {"icon_128x128.png", 'ic07'},
    {"icon_128x128@2x.png", 'ic13'},
    {"icon_256x256.png", 'ic08'},
    {"icon_256x256@2x.png", 'ic14'},
    {"icon_512x512.png", 'ic09'},
    {"icon_512x512@2x.png", 'ic10'}
};

bool IsEmpty(Path path) {
  return path.path[0] == '\0';
}

void PrintError(const char* error) {
  fprintf(stderr, "Error: %s\n", error);
}

void PrintSystemError() {
  perror("Error");
}

void PrintUsage(const char* own_path) {
  fprintf(stderr, "Usage: %s [file.icns]\n", own_path);
}

char* Basename(const char* path, char* basename) {
  if (!path || *path == '\0') {
    strlcpy(basename, ".", sizeof("."));
    return basename;
  }

  size_t length = strlen(path);
  const char* end;
  for (end = path + length - 1; end > path && *end == '/'; end--) {}
  const char* begin;
  for (begin = end; begin > path && *(begin - 1) != '/'; begin--) {}

  size_t base_length = (end - begin) + 1;
  if (base_length >= MAXPATHLEN)
    return NULL;

  strlcpy(basename, begin, base_length + 1);
  return basename;
}

const char* IcnsFromArguments(int argc, char* argv[]) {
  if (argc < 2) {
    PrintError("No path given to icns file.");
    PrintUsage(argv[0]);
    return NULL;
  } else if (argc > 2) {
    PrintError("Too many arguments.");
    PrintUsage(argv[0]);
    return NULL;
  }

  return argv[1];
}

uint32_t ReadUint32(FILE* file) {
  uint32_t read;
  if (fread(&read, sizeof(read), 1, file) != 1)
    return 0;

  return ntohl(read);
}

FILE* OpenIcnsFileForReading(const char* icns_path) {
  FILE* icns = fopen(icns_path, "r");
  if (!icns) {
    PrintSystemError();
    return NULL;
  }

  uint32_t header = ReadUint32(icns);
  if (header != kMagicHeader) {
    PrintError("This doesn't look like an Apple .icns file.");
    fclose(icns);
    return NULL;
  }

  uint32_t size = ReadUint32(icns);
  if (!size) {
    PrintError("This looks like an empty .icns file.");
    fclose(icns);
    return NULL;
  }

  return icns;
}

Path GetIconsetPath(const char* icns_path) {
  Path path = {0};
  if (!Basename(icns_path, path.path)) {
    PrintError("Can't determine name of icns file");
    return path;
  }

  char* extension = strstr(path.path, kIcnsExtension);
  if (!extension) {
    PrintError("Can't find .icns extension on input file");
    path.path[0] = '\0';
    return path;
  }

  strlcpy(extension, kIconsetExtension, MAXPATHLEN - (extension - path.path));
  return path;
}

const char* GetFilenameFromType(uint32_t type) {
  for (size_t i = 0; i < (sizeof(kIconTypes) / sizeof(*kIconTypes)); i++) {
    if (kIconTypes[i].icon_type == type)
      return kIconTypes[i].icon_filename;
  }

  return NULL;

}

bool CopyIconToIconset(FILE* icns, Path iconset_path) {
  uint32_t header = ReadUint32(icns);
  if (!header && feof(icns))
    return true;

  uint32_t size = ReadUint32(icns);
  if (size <= 8) {
    PrintError("Invalid size in .icns file");
    return false;
  }
  size -= 8;

  const char* icon_filename = GetFilenameFromType(header);
  char* iconset_path_end = iconset_path.path + strlen(iconset_path.path);
  *iconset_path_end++ = '/';
  if (icon_filename) {
    strlcpy(iconset_path_end, icon_filename,
            MAXPATHLEN - (iconset_path_end - iconset_path.path));
  } else {
    char unknown_format[] = {(header >> 24) & 0xff, (header >> 16) & 0xff,
                             (header >> 8) & 0xff, header & 0xff, '\0'};

    iconset_path_end +=
        strlcpy(iconset_path_end, kUnknownFormatFilename,
                MAXPATHLEN - (iconset_path_end - iconset_path.path));
    strlcpy(iconset_path_end, unknown_format,
            MAXPATHLEN - (iconset_path_end - iconset_path.path));
  }

  FILE* target = fopen(iconset_path.path, "w");
  if (!target) {
    PrintSystemError();
    return false;
  }

  uint8_t buffer[kBufferSize];
  while (size > 0) {
    size_t to_read = size > kBufferSize ? kBufferSize : size;
    if (fread(buffer, 1, to_read, icns) != to_read ||
        fwrite(buffer, 1, to_read, target) != to_read) {
      PrintError("Error copying from .icns file to iconset");
      fclose(target);
      return false;
    }
    size -= to_read;
  }

  fclose(target);
  return true;
}

bool CreateIconsetFromIcns(const char* icns_path) {
  FILE* icns = OpenIcnsFileForReading(icns_path);
  if (!icns)
    return false;

  Path iconset_path = GetIconsetPath(icns_path);
  if (IsEmpty(iconset_path)) {
    fclose(icns);
    return false;
  }

  if (mkdir(iconset_path.path, 0777)) {
    PrintSystemError();
    fclose(icns);
    return false;
  }

  while (!feof(icns)) {
    if (!CopyIconToIconset(icns, iconset_path)) {
      fclose(icns);
      return false;
    }
  }

  fclose(icns);
  return true;
}

int main(int argc, char* argv[]) {
  const char* icns_path = IcnsFromArguments(argc, argv);
  if (!icns_path)
    return -1;

  if (!CreateIconsetFromIcns(icns_path))
    return -1;

  return 0;
}
