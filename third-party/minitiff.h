#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "minilibs.h"


#define TIFFTAG_SUBFILETYPE 254
#define TIFFTAG_IMAGEWIDTH 256
#define TIFFTAG_IMAGELENGTH 257
#define TIFFTAG_BITSPERSAMPLE 258
#define TIFFTAG_COMPRESSION 259
#define TIFFTAG_PHOTOMETRIC 262
#define TIFFTAG_IMAGEDESCRIPTION 270
#define TIFFTAG_SOFTWARE 305
#define TIFFTAG_DATETIME 306
#define TIFFTAG_SAMPLESPERPIXEL 277
#define TIFFTAG_ROWSPERSTRIP 278
#define TIFFTAG_PLANARCONFIG 284
#define TIFFTAG_RESOLUTIONUNIT 296
#define TIFFTAG_XRESOLUTION 282
#define TIFFTAG_YRESOLUTION 283
#define TIFFTAG_SAMPLEFORMAT 339
#define TIFFTAG_STRIPOFFSETS 273
#define TIFFTAG_STRIPBYTECOUNTS 279

#define TIFF_BYTE 1
#define TIFF_ASCII 2
#define TIFF_SHORT 3
#define TIFF_LONG 4
#define TIFF_RATIONAL 5

typedef struct {
    uint32_t offset;
    uint32_t byteCount;
} StripInfo;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t bitsPerSample;
    uint16_t compression;
    uint16_t photometric;
    uint16_t samplesPerPixel;
    uint32_t rowsPerStrip;
    uint16_t planarConfig;
    uint16_t sampleFormat;
    StripInfo stripInfo;
    char imageDescription[256];
    char software[256];
    char dateTime[20];
    float xResolution;
    float yResolution;
    uint16_t resolutionUnit;
    uint32_t subfileType;
} DirectoryInfo;

typedef struct {
    DirectoryInfo* directories;
    uint16_t depth;
    size_t dataSize;
    void* data;
    bool isValid;
    char errorMsg[256];
} TiffImage;

PRIVATE uint32_t readBytes(FILE* fp, int count, int littleEndian) {
    uint32_t value = 0;
    uint8_t byte;

    if (littleEndian) {
        for (int i = 0; i < count; i++) {
            if (fread(&byte, 1, 1, fp) != 1) return 0;
            value |= ((uint32_t)byte << (i * 8));
        }
    } else {
        for (int i = 0; i < count; i++) {
            if (fread(&byte, 1, 1, fp) != 1) return 0;
            value = (value << 8) | byte;
        }
    }

    return value;
}

PRIVATE void readString(FILE* fp, char* str, uint32_t offset, uint32_t count, long currentPos) {
    long savedPos = ftell(fp);
    fseek(fp, offset, SEEK_SET);
    fread(str, 1, count - 1, fp);
    str[count - 1] = '\0';
    fseek(fp, savedPos, SEEK_SET);
}

PRIVATE float readRational(FILE* fp, uint32_t offset, int littleEndian, long currentPos) {
    long savedPos = ftell(fp);
    fseek(fp, offset, SEEK_SET);
    uint32_t numerator = readBytes(fp, 4, littleEndian);
    uint32_t denominator = readBytes(fp, 4, littleEndian);
    fseek(fp, savedPos, SEEK_SET);
    return denominator ? (float)numerator / denominator : 0.0f;
}

PRIVATE void readIFDEntry(FILE* fp, DirectoryInfo* dir, int littleEndian, long ifdStart) {
    uint16_t tag = readBytes(fp, 2, littleEndian);
    uint16_t type = readBytes(fp, 2, littleEndian);
    uint32_t count = readBytes(fp, 4, littleEndian);
    uint32_t valueOffset = readBytes(fp, 4, littleEndian);

    long currentPos = ftell(fp);

    switch (tag) {
        case TIFFTAG_SUBFILETYPE:
            dir->subfileType = valueOffset;
            break;
        case TIFFTAG_IMAGEWIDTH:
            dir->width = (type == TIFF_SHORT) ? (uint16_t)valueOffset : valueOffset;
            break;
        case TIFFTAG_IMAGELENGTH:
            dir->height = (type == TIFF_SHORT) ? (uint16_t)valueOffset : valueOffset;
            break;
        case TIFFTAG_BITSPERSAMPLE:
            dir->bitsPerSample = (uint16_t)valueOffset;
            break;
        case TIFFTAG_COMPRESSION:
            dir->compression = (uint16_t)valueOffset;
            break;
        case TIFFTAG_PHOTOMETRIC:
            dir->photometric = (uint16_t)valueOffset;
            break;
        case TIFFTAG_IMAGEDESCRIPTION:
            readString(fp, dir->imageDescription, valueOffset, count, currentPos);
            break;
        case TIFFTAG_SOFTWARE:
            readString(fp, dir->software, valueOffset, count, currentPos);
            break;
        case TIFFTAG_DATETIME:
            readString(fp, dir->dateTime, valueOffset, count, currentPos);
            break;
        case TIFFTAG_SAMPLESPERPIXEL:
            dir->samplesPerPixel = (uint16_t)valueOffset;
            break;
        case TIFFTAG_ROWSPERSTRIP:
            dir->rowsPerStrip = (type == TIFF_SHORT) ? (uint16_t)valueOffset : valueOffset;
            break;
        case TIFFTAG_PLANARCONFIG:
            dir->planarConfig = (uint16_t)valueOffset;
            break;
        case TIFFTAG_XRESOLUTION:
            dir->xResolution = readRational(fp, valueOffset, littleEndian, currentPos);
            break;
        case TIFFTAG_YRESOLUTION:
            dir->yResolution = readRational(fp, valueOffset, littleEndian, currentPos);
            break;
        case TIFFTAG_RESOLUTIONUNIT:
            dir->resolutionUnit = (uint16_t)valueOffset;
            break;
        case TIFFTAG_SAMPLEFORMAT:
            dir->sampleFormat = (uint16_t)valueOffset;
            break;
        case TIFFTAG_STRIPOFFSETS:
            dir->stripInfo.offset = (type == TIFF_SHORT) ? (uint16_t)valueOffset : valueOffset;
            break;
        case TIFFTAG_STRIPBYTECOUNTS:
            dir->stripInfo.byteCount = (type == TIFF_SHORT) ? (uint16_t)valueOffset : valueOffset;
            break;
    }
}

PRIVATE bool validateDirectory(DirectoryInfo* dir, TiffImage* img) {
    if (dir->width == 0 || dir->height == 0) {
        snprintf(img->errorMsg, sizeof(img->errorMsg), "Invalid dimensions");
        return false;
    }

    if (dir->bitsPerSample != 8 && dir->bitsPerSample != 16) {
        snprintf(img->errorMsg, sizeof(img->errorMsg),
                "Unsupported bits per sample: %d", dir->bitsPerSample);
        return false;
    }

    if (dir->compression != 1) {
        snprintf(img->errorMsg, sizeof(img->errorMsg),
                "Unsupported compression: %d", dir->compression);
        return false;
    }

    if (dir->samplesPerPixel != 1) {
        snprintf(img->errorMsg, sizeof(img->errorMsg),
                "Only single channel images supported");
        return false;
    }

    if (dir->planarConfig != 1) {
        snprintf(img->errorMsg, sizeof(img->errorMsg),
                "Only contiguous data supported");
        return false;
    }

    size_t expectedSize = dir->width * dir->height * (dir->bitsPerSample / 8);
    if (dir->stripInfo.byteCount != expectedSize) {
        snprintf(img->errorMsg, sizeof(img->errorMsg), "Data size mismatch");
        return false;
    }

    return true;
}

PUBLIC TiffImage* readTIFF(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return NULL;

    TiffImage* img = calloc(1, sizeof(TiffImage));
    if (!img) {
        fclose(fp);
        return NULL;
    }

    img->isValid = true;

    uint16_t byteOrder = readBytes(fp, 2, 1);
    int littleEndian = (byteOrder == 0x4949);

    if (byteOrder != 0x4949 && byteOrder != 0x4D4D) {
        img->isValid = false;
        snprintf(img->errorMsg, sizeof(img->errorMsg), "Invalid byte order marker");
        fclose(fp);
        return img;
    }

    if (readBytes(fp, 2, littleEndian) != 42) {
        img->isValid = false;
        snprintf(img->errorMsg, sizeof(img->errorMsg), "Invalid TIFF version");
        fclose(fp);
        return img;
    }

    // First pass: count directories
    uint32_t ifdOffset = readBytes(fp, 4, littleEndian);
    img->depth = 0;
    uint32_t nextIFD = ifdOffset;

    while (nextIFD != 0) {
        img->depth++;
        fseek(fp, nextIFD, SEEK_SET);
        uint16_t numEntries = readBytes(fp, 2, littleEndian);
        fseek(fp, 12 * numEntries, SEEK_CUR);  // Skip entries
        nextIFD = readBytes(fp, 4, littleEndian);
    }

    // Allocate directory info array
    img->directories = calloc(img->depth, sizeof(DirectoryInfo));
    if (!img->directories) {
        img->isValid = false;
        snprintf(img->errorMsg, sizeof(img->errorMsg), "Memory allocation failed");
        fclose(fp);
        return img;
    }

    // Second pass: read directory information
    nextIFD = ifdOffset;
    int dirIndex = 0;

    while (nextIFD != 0 && img->isValid) {
        DirectoryInfo* currentDir = &img->directories[dirIndex];

        // Set defaults
        currentDir->samplesPerPixel = 1;
        currentDir->planarConfig = 1;
        currentDir->sampleFormat = 1;
        currentDir->compression = 1;

        fseek(fp, nextIFD, SEEK_SET);
        long ifdStart = ftell(fp);

        uint16_t numEntries = readBytes(fp, 2, littleEndian);

        for (int i = 0; i < numEntries && img->isValid; i++) {
            readIFDEntry(fp, currentDir, littleEndian, ifdStart);
        }

        if (!validateDirectory(currentDir, img)) {
            img->isValid = false;
            break;
        }

        nextIFD = readBytes(fp, 4, littleEndian);
        dirIndex++;
    }

    if (img->isValid) {
        DirectoryInfo* firstDir = &img->directories[0];
        size_t sliceSize = firstDir->width * firstDir->height * (firstDir->bitsPerSample / 8);
        img->dataSize = sliceSize * img->depth;
        img->data = malloc(img->dataSize);

        if (!img->data) {
            img->isValid = false;
            snprintf(img->errorMsg, sizeof(img->errorMsg), "Memory allocation failed");
        } else {
            for (int i = 0; i < img->depth && img->isValid; i++) {
                DirectoryInfo* dir = &img->directories[i];
                fseek(fp, dir->stripInfo.offset, SEEK_SET);
                size_t bytesRead = fread((uint8_t*)img->data + (i * sliceSize), 1,
                                       dir->stripInfo.byteCount, fp);
                if (bytesRead != dir->stripInfo.byteCount) {
                    img->isValid = false;
                    snprintf(img->errorMsg, sizeof(img->errorMsg),
                            "Failed to read image data for directory %d", i);
                }
            }
        }
    }

    fclose(fp);
    return img;
}

PUBLIC void freeTIFF(TiffImage* img) {
    if (img) {
        free(img->directories);
        free(img->data);
        free(img);
    }
}

PRIVATE const char* getCompressionName(uint16_t compression) {
    switch (compression) {
        case 1: return "None";
        case 2: return "CCITT modified Huffman RLE";
        case 3: return "CCITT Group 3 fax encoding";
        case 4: return "CCITT Group 4 fax encoding";
        case 5: return "LZW";
        case 6: return "JPEG (old-style)";
        case 7: return "JPEG";
        case 8: return "Adobe Deflate";
        case 32773: return "PackBits compression";
        default: return "Unknown";
    }
}

PRIVATE const char* getPhotometricName(uint16_t photometric) {
    switch (photometric) {
        case 0: return "min-is-white";
        case 1: return "min-is-black";
        case 2: return "RGB";
        case 3: return "palette color";
        case 4: return "transparency mask";
        case 5: return "CMYK";
        case 6: return "YCbCr";
        case 8: return "CIELab";
        default: return "Unknown";
    }
}

PRIVATE const char* getPlanarConfigName(uint16_t config) {
    switch (config) {
        case 1: return "single image plane";
        case 2: return "separate image planes";
        default: return "Unknown";
    }
}

PRIVATE const char* getSampleFormatName(uint16_t format) {
    switch (format) {
        case 1: return "unsigned integer";
        case 2: return "signed integer";
        case 3: return "IEEE floating point";
        case 4: return "undefined";
        default: return "Unknown";
    }
}

PRIVATE const char* getResolutionUnitName(uint16_t unit) {
    switch (unit) {
        case 1: return "unitless";
        case 2: return "inches";
        case 3: return "centimeters";
        default: return "Unknown";
    }
}

PUBLIC void printTIFFTags(const TiffImage* img, int directory) {
    if (!img || !img->directories || directory >= img->depth) return;

    const DirectoryInfo* dir = &img->directories[directory];

    printf("\n=== TIFF directory %d ===\n", directory);
    printf("TIFF Directory %d\n", directory);

    if (dir->subfileType != 0) {
        printf("  Subfile Type: (%d = 0x%x)\n", dir->subfileType, dir->subfileType);
    }

    printf("  Image Width: %u Image Length: %u\n", dir->width, dir->height);

    if (dir->xResolution != 0 || dir->yResolution != 0) {
        printf("  Resolution: %g, %g (%s)\n",
               dir->xResolution, dir->yResolution,
               getResolutionUnitName(dir->resolutionUnit));
    }

    printf("  Bits/Sample: %u\n", dir->bitsPerSample);
    printf("  Sample Format: %s\n", getSampleFormatName(dir->sampleFormat));
    printf("  Compression Scheme: %s\n", getCompressionName(dir->compression));
    printf("  Photometric Interpretation: %s\n", getPhotometricName(dir->photometric));
    printf("  Samples/Pixel: %u\n", dir->samplesPerPixel);

    if (dir->rowsPerStrip) {
        printf("  Rows/Strip: %u\n", dir->rowsPerStrip);
    }

    printf("  Planar Configuration: %s\n", getPlanarConfigName(dir->planarConfig));

    if (dir->imageDescription[0]) {
        printf("  ImageDescription: %s\n", dir->imageDescription);
    }
    if (dir->software[0]) {
        printf("  Software: %s\n", dir->software);
    }
    if (dir->dateTime[0]) {
        printf("  DateTime: %s\n", dir->dateTime);
    }
}

PUBLIC void printAllTIFFTags(const TiffImage* img) {
    if (!img) {
        printf("Error: NULL TIFF image\n");
        return;
    }

    if (!img->isValid) {
        printf("Error reading TIFF: %s\n", img->errorMsg);
        return;
    }

    for (int i = 0; i < img->depth; i++) {
        printTIFFTags(img, i);
    }
}


PRIVATE size_t getTIFFDirectorySize(const TiffImage* img, int directory) {
    if (!img || !img->isValid || !img->directories || directory >= img->depth) {
        return 0;
    }

    const DirectoryInfo* dir = &img->directories[directory];
    return dir->width * dir->height * (dir->bitsPerSample / 8);
}

PRIVATE void* readTIFFDirectoryData(const TiffImage* img, int directory) {

    size_t bufferSize = getTIFFDirectorySize(img, directory);
    void* buffer = malloc(bufferSize);

    if (!img || !img->isValid || !img->directories || !buffer || directory >= img->depth) {
        return NULL;
    }

    const DirectoryInfo* dir = &img->directories[directory];
    size_t sliceSize = dir->width * dir->height * (dir->bitsPerSample / 8);

    if (bufferSize < sliceSize) {
        return NULL;
    }

    size_t offset = sliceSize * directory;
    memcpy(buffer, (uint8_t*)img->data + offset, sliceSize);

    return buffer;
}

PRIVATE uint16_t getTIFFPixel16FromBuffer(const uint16_t* buffer, int y, int x, int width) {
    return buffer[ y * width + x];
}

PRIVATE uint8_t getTIFFPixel8FromBuffer(const uint8_t* buffer, int y, int x, int width) {
    return buffer[y * width + x];
}


PRIVATE void writeBytes(FILE* fp, uint32_t value, int count, int littleEndian) {
    if (littleEndian) {
        for (int i = 0; i < count; i++) {
            uint8_t byte = (value >> (i * 8)) & 0xFF;
            fwrite(&byte, 1, 1, fp);
        }
    } else {
        for (int i = count - 1; i >= 0; i--) {
            uint8_t byte = (value >> (i * 8)) & 0xFF;
            fwrite(&byte, 1, 1, fp);
        }
    }
}

PRIVATE void writeString(FILE* fp, const char* str, uint32_t offset) {
    fseek(fp, offset, SEEK_SET);
    size_t len = strlen(str);
    fwrite(str, 1, len + 1, fp);  // Include null terminator
}

PRIVATE void writeRational(FILE* fp, float value, uint32_t offset, int littleEndian) {
    fseek(fp, offset, SEEK_SET);
    uint32_t numerator = (uint32_t)(value * 1000);
    uint32_t denominator = 1000;
    writeBytes(fp, numerator, 4, littleEndian);
    writeBytes(fp, denominator, 4, littleEndian);
}

PRIVATE void getCurrentDateTime(char* dateTime) {
    time_t now;
    struct tm* timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    strftime(dateTime, 20, "%Y:%m:%d %H:%M:%S", timeinfo);
}

PRIVATE uint32_t writeIFDEntry(FILE* fp, uint16_t tag, uint16_t type, uint32_t count,
                             uint32_t value, int littleEndian) {
    writeBytes(fp, tag, 2, littleEndian);
    writeBytes(fp, type, 2, littleEndian);
    writeBytes(fp, count, 4, littleEndian);
    writeBytes(fp, value, 4, littleEndian);
    return 12;  // Size of IFD entry
}

PUBLIC int writeTIFF(const char* filename, const TiffImage* img, bool littleEndian) {
    if (!img || !img->directories || !img->data || !img->isValid) return 1;

    FILE* fp = fopen(filename, "wb");
    if (!fp) return 1;

    // Write header
    writeBytes(fp, littleEndian ? 0x4949 : 0x4D4D, 2, 1);  // Byte order marker
    writeBytes(fp, 42, 2, littleEndian);                    // TIFF version

    uint32_t ifdOffset = 8;  // Start first IFD after header
    writeBytes(fp, ifdOffset, 4, littleEndian);

    // Calculate space needed for string and rational values
    uint32_t extraDataOffset = ifdOffset;
    for (int d = 0; d < img->depth; d++) {
        extraDataOffset += 2 + (12 * 17) + 4;  // Directory entry count + entries + next IFD pointer
    }

    // Write each directory
    for (int d = 0; d < img->depth; d++) {
        const DirectoryInfo* dir = &img->directories[d];

        // Position at IFD start
        fseek(fp, ifdOffset, SEEK_SET);

        // Write number of directory entries
        writeBytes(fp, 17, 2, littleEndian);  // Number of IFD entries

        // Write directory entries
        writeIFDEntry(fp, TIFFTAG_SUBFILETYPE, TIFF_LONG, 1, dir->subfileType, littleEndian);
        writeIFDEntry(fp, TIFFTAG_IMAGEWIDTH, TIFF_LONG, 1, dir->width, littleEndian);
        writeIFDEntry(fp, TIFFTAG_IMAGELENGTH, TIFF_LONG, 1, dir->height, littleEndian);
        writeIFDEntry(fp, TIFFTAG_BITSPERSAMPLE, TIFF_SHORT, 1, dir->bitsPerSample, littleEndian);
        writeIFDEntry(fp, TIFFTAG_COMPRESSION, TIFF_SHORT, 1, dir->compression, littleEndian);
        writeIFDEntry(fp, TIFFTAG_PHOTOMETRIC, TIFF_SHORT, 1, dir->photometric, littleEndian);
        writeIFDEntry(fp, TIFFTAG_SAMPLESPERPIXEL, TIFF_SHORT, 1, dir->samplesPerPixel, littleEndian);
        writeIFDEntry(fp, TIFFTAG_ROWSPERSTRIP, TIFF_LONG, 1, dir->rowsPerStrip, littleEndian);
        writeIFDEntry(fp, TIFFTAG_PLANARCONFIG, TIFF_SHORT, 1, dir->planarConfig, littleEndian);
        writeIFDEntry(fp, TIFFTAG_SAMPLEFORMAT, TIFF_SHORT, 1, dir->sampleFormat, littleEndian);

        // Write resolution entries
        writeIFDEntry(fp, TIFFTAG_XRESOLUTION, TIFF_RATIONAL, 1, extraDataOffset, littleEndian);
        writeRational(fp, dir->xResolution, extraDataOffset, littleEndian);
        extraDataOffset += 8;

        writeIFDEntry(fp, TIFFTAG_YRESOLUTION, TIFF_RATIONAL, 1, extraDataOffset, littleEndian);
        writeRational(fp, dir->yResolution, extraDataOffset, littleEndian);
        extraDataOffset += 8;

        writeIFDEntry(fp, TIFFTAG_RESOLUTIONUNIT, TIFF_SHORT, 1, dir->resolutionUnit, littleEndian);

        // Write metadata strings if present
        if (dir->imageDescription[0]) {
            size_t len = strlen(dir->imageDescription) + 1;
            writeIFDEntry(fp, TIFFTAG_IMAGEDESCRIPTION, TIFF_ASCII, len, extraDataOffset, littleEndian);
            writeString(fp, dir->imageDescription, extraDataOffset);
            extraDataOffset += len;
        }

        if (dir->software[0]) {
            size_t len = strlen(dir->software) + 1;
            writeIFDEntry(fp, TIFFTAG_SOFTWARE, TIFF_ASCII, len, extraDataOffset, littleEndian);
            writeString(fp, dir->software, extraDataOffset);
            extraDataOffset += len;
        }

        if (dir->dateTime[0]) {
            writeIFDEntry(fp, TIFFTAG_DATETIME, TIFF_ASCII, 20, extraDataOffset, littleEndian);
            writeString(fp, dir->dateTime, extraDataOffset);
            extraDataOffset += 20;
        }

        // Calculate strip size and write strip information
        size_t stripSize = dir->width * dir->height * (dir->bitsPerSample / 8);
        writeIFDEntry(fp, TIFFTAG_STRIPOFFSETS, TIFF_LONG, 1, extraDataOffset, littleEndian);
        writeIFDEntry(fp, TIFFTAG_STRIPBYTECOUNTS, TIFF_LONG, 1, stripSize, littleEndian);

        // Write image data
        fseek(fp, extraDataOffset, SEEK_SET);
        size_t offset = stripSize * d;
        fwrite((uint8_t*)img->data + offset, 1, stripSize, fp);
        extraDataOffset += stripSize;

        // Write next IFD offset or 0 if last directory
        uint32_t nextIFD = (d < img->depth - 1) ? extraDataOffset : 0;
        writeBytes(fp, nextIFD, 4, littleEndian);

        ifdOffset = nextIFD;
    }

    fclose(fp);
    return 0;
}

PUBLIC TiffImage* createTIFF(uint32_t width, uint32_t height, uint16_t depth,
                           uint16_t bitsPerSample) {
    TiffImage* img = calloc(1, sizeof(TiffImage));
    if (!img) return NULL;

    img->depth = depth;
    img->directories = calloc(depth, sizeof(DirectoryInfo));
    if (!img->directories) {
        free(img);
        return NULL;
    }

    img->dataSize = width * height * (bitsPerSample / 8) * depth;
    img->data = calloc(1, img->dataSize);
    if (!img->data) {
        free(img->directories);
        free(img);
        return NULL;
    }

    // Initialize each directory
    for (int i = 0; i < depth; i++) {
        DirectoryInfo* dir = &img->directories[i];
        dir->width = width;
        dir->height = height;
        dir->bitsPerSample = bitsPerSample;
        dir->compression = 1;  // No compression
        dir->photometric = 1;  // min-is-black
        dir->samplesPerPixel = 1;
        dir->rowsPerStrip = height;
        dir->planarConfig = 1;
        dir->sampleFormat = 1;  // unsigned integer
        dir->xResolution = 72.0f;
        dir->yResolution = 72.0f;
        dir->resolutionUnit = 2;  // ?
        dir->subfileType = 0;

        getCurrentDateTime(dir->dateTime);
    }

    img->isValid = true;
    return img;
}