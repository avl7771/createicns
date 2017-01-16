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

// This is a generator for the Apple Icon Image Format (.icns) which reads PNGs
// without changing them.
//
// It's run like this: 'createicons x.iconset' and outputs a file 'x.icns'.
//
// The input is a .iconset directory with files conforming to the naming scheme
// for .iconset directories. It reads a 'complete' set of PNG icons as described
// here:
// https://developer.apple.com/library/content/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html
//
// To generate a .iconset directory from an existing x.icns file, use
//   'iconutil -c iconset x.icns'
//
// This tool is similar to running 'iconutil -c icns x.iconset', except it
// doesn't change the PNG images in any way.

#include <arpa/inet.h>
#include <dirent.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

// Magic values for headers were found at
// https://en.wikipedia.org/wiki/Apple_Icon_Image_format.

enum kBufferSize { kBufferSize = 1024 };
static const char kIconsetExtension[] = ".iconset";
static const char kIcnsExtension[] = ".icns";
static const uint32_t kMagicHeader = 'icns';

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

void PrintError(const char* error) {
  fprintf(stderr, "Error: %s\n", error);
}

void PrintSystemError() {
  perror("Error");
}

void PrintUsage(const char* own_path) {
  fprintf(stderr, "Usage: %s [iconset]\n", own_path);
}

const char* IconsetFromArguments(int argc, char* argv[]) {
  if (argc < 2) {
    PrintError("No path given to iconset directory.");
    PrintUsage(argv[0]);
    return NULL;
  } else if (argc > 2) {
    PrintError("Too many arguments.");
    PrintUsage(argv[0]);
    return NULL;
  }

  return argv[1];
}

bool WriteUint32(uint32_t to_write, FILE* file) {
  uint32_t msb_first = htonl(to_write);
  return fwrite(&msb_first, sizeof(msb_first), 1, file) == 1;
}

FILE* OpenIcnsFileForIconset(const char* iconset_path) {
  char path[MAXPATHLEN];
  if (!basename_r(iconset_path, path)) {
    PrintSystemError();
    return NULL;
  }

  const size_t path_length = strlen(path);
  const size_t extension_length = sizeof(kIconsetExtension) - 1;
  const size_t base_path_length = path_length - extension_length;

  if (path_length <= extension_length ||
      strncmp(path + base_path_length, kIconsetExtension, extension_length) !=
          0) {
    PrintError("Need .iconset directory as input.");
    return NULL;
  }

  memcpy(path + base_path_length, kIcnsExtension, sizeof(kIcnsExtension));
  FILE* file = fopen(path, "w");
  if (!file) {
    PrintSystemError();
    return NULL;
  }

  // Every .icns file starts with a magic header (4 bytes) and the total size
  // including the header (4 bytes). Since we don't know the size yet, we'll
  // overwrite this in WriteIcnsFileMetadata() with the real value.
  if (!WriteUint32(kMagicHeader, file) ||
      !WriteUint32(0, file)) {
    PrintSystemError();
    fclose(file);
    return NULL;
  }

  return file;
}

uint32_t FindIconType(const char* icon_filename) {
  for (size_t i = 0; i < (sizeof(kIconTypes) / sizeof(*kIconTypes)); i++) {
    if (strcmp(kIconTypes[i].icon_filename, icon_filename) == 0)
      return kIconTypes[i].icon_type;
  }

  return 0;
}

bool WriteIconToFile(const char *iconset_path, const char *icon_filename,
                     uint32_t icon_type, FILE *outfile) {
  const size_t iconset_path_length = strlen(iconset_path);
  const size_t icon_filename_length = strlen(icon_filename);
  char* icon_path = malloc(iconset_path_length + icon_filename_length + 2);

  memcpy(icon_path, iconset_path, iconset_path_length);
  icon_path[iconset_path_length] = '/';
  memcpy(icon_path + iconset_path_length + 1, icon_filename,
         icon_filename_length + 1);

  FILE* infile = fopen(icon_path, "r");
  free(icon_path);
  if (!infile) {
    PrintSystemError();
    return false;
  }

  // For every icon, we put a magic header (4 bytes) and the total size of the
  // icon following including the header (4 bytes), followed by the icon
  // itself.
  long size = 0;
  if (fseek(infile, 0L, SEEK_END) < 0 ||
      (size = ftell(infile)) < 0 ||
      fseek(infile, 0L, SEEK_SET) < 0) {
    PrintSystemError();
    fclose(infile);
    return false;
  }

  if (!WriteUint32(icon_type, outfile) ||
      !WriteUint32(size + 8, outfile)) {
    PrintSystemError();
    fclose(infile);
    return false;
  }

  uint8_t buffer[kBufferSize];
  size_t read;
  do {
    read = fread(buffer, 1, kBufferSize, infile);
    if ((read < kBufferSize && ferror(infile)) ||
        fwrite(buffer, 1, read, outfile) < read) {
      PrintSystemError();
      fclose(infile);
      return false;
    }
  } while (read == kBufferSize);

  fclose(infile);
  return true;
}

bool WriteIcnsFileMetadata(FILE* file) {
  long size = ftell(file);
  return size >= 0 && fseek(file, 4L, SEEK_SET) == 0 && WriteUint32(size, file);
}

bool CreateIcnsFromIconset(const char* iconset_path) {
  DIR* iconset = opendir(iconset_path);
  if (!iconset) {
    PrintSystemError();
    return false;
  }

  FILE* icns = OpenIcnsFileForIconset(iconset_path);
  if (!icns)
    return false;

  for (struct dirent *entry = readdir(iconset); entry;
       entry = readdir(iconset)) {
    if (entry->d_name[0] == '.')
      continue;

    uint32_t icon_type = FindIconType(entry->d_name);
    if (!icon_type) {
      fprintf(stderr, "Warning: Don't know icon type for %s, skipping\n",
              entry->d_name);
      continue;
    }

    if (!WriteIconToFile(iconset_path, entry->d_name, icon_type, icns)) {
      fclose(icns);
      return false;
    }
  }

  if (!WriteIcnsFileMetadata(icns)) {
    fclose(icns);
    return false;
  }

  fclose(icns);
  return true;
}

int main(int argc, char* argv[]) {
  const char* iconset_path = IconsetFromArguments(argc, argv);
  if (!iconset_path)
    return -1;

  if (!CreateIcnsFromIconset(iconset_path))
    return -1;

  return 0;
}
