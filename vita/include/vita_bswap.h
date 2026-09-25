#ifndef _VITA_BSWAP_H_
#define _VITA_BSWAP_H_

#ifdef TARGET_VITA

enum {
    SWAP_NONE = 0,
    SWAP_U16 = 1,
    SWAP_VTX = 2,
    SWAP_U32 = 3
};

static inline void vita_bswap_u16_array(void* data, unsigned int size) {
    unsigned short* arr = (unsigned short*)data;
    for (unsigned int i = 0; i < size / 2; i++) {
        arr[i] = __builtin_bswap16(arr[i]);
    }
}

static inline void vita_bswap_u32_array(void* data, unsigned int size) {
    unsigned int* arr = (unsigned int*)data;
    for (unsigned int i = 0; i < size / 4; i++) {
        arr[i] = __builtin_bswap32(arr[i]);
    }
}

static inline void vita_bswap_vtx_array(void* data, unsigned int size) {
    // 16-byte stride, first 12 bytes as u16 pairs
    unsigned char* ptr = (unsigned char*)data;
    for (unsigned int i = 0; i < size / 16; i++) {
        unsigned short* u16ptr = (unsigned short*)(ptr + i * 16);
        u16ptr[0] = __builtin_bswap16(u16ptr[0]);
        u16ptr[1] = __builtin_bswap16(u16ptr[1]);
        u16ptr[2] = __builtin_bswap16(u16ptr[2]);
        u16ptr[3] = __builtin_bswap16(u16ptr[3]);
        u16ptr[4] = __builtin_bswap16(u16ptr[4]);
        u16ptr[5] = __builtin_bswap16(u16ptr[5]);
    }
}

static inline void vita_bswap_region(void* data, unsigned int size, int type) {
    if (!data || size == 0) return;
    switch (type) {
        case SWAP_U16: vita_bswap_u16_array(data, size); break;
        case SWAP_VTX: vita_bswap_vtx_array(data, size); break;
        case SWAP_U32: vita_bswap_u32_array(data, size); break;
        default: break;
    }
}

#endif // TARGET_VITA
#endif // _VITA_BSWAP_H_

static inline void vita_bswap_gx_texture_header(void* data) {
    if (!data) return;
    // numLevels (U32), format (U32)
    vita_bswap_region(data, 8, SWAP_U32);
    // numBits[4] and missingTexture (1) and padding (1) - total 6 bytes unswapped
    // width (U16), height (U16)
    vita_bswap_region((char*)data + 0x0E, 4, SWAP_U16);
    // numEntries (U32), pad[2] (U32)
    vita_bswap_region((char*)data + 0x14, 12, SWAP_U32);
}
