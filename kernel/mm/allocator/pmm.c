#include <mm/pmm.h>
#include <libk/string.h>
#include <drivers/vga.h>

// Bellek haritası (Bitmap) için değişkenler
static uint32_t *pmm_bitmap = NULL;
static size_t pmm_max_blocks = 0;
static size_t pmm_used_blocks = 0;

// Bitmap üzerinde bit işlemleri için yardımcılar
static inline void bitmap_set(uint32_t bit) {
  pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

static inline void bitmap_unset(uint32_t bit) {
  pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

static inline bool bitmap_test(uint32_t bit) {
  return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

// Boş bir blok (bit) bul
static int bitmap_find_first_free(void) {
  for (size_t i = 0; i < pmm_max_blocks / 32; i++) {
    if (pmm_bitmap[i] != 0xFFFFFFFF) {
      for (int j = 0; j < 32; j++) {
        if (!(pmm_bitmap[i] & (1 << j))) {
          return i * 32 + j;
        }
      }
    }
  }
  return -1;
}

// PMM'i başlat
void pmm_init(size_t mem_size, uint32_t bitmap_addr) {
  pmm_max_blocks = mem_size / PMM_BLOCK_SIZE;
  pmm_bitmap = (uint32_t *)bitmap_addr;
  pmm_used_blocks = pmm_max_blocks; // Başta tüm belleği "dolu" sayalım

  // Bitmap'i temizle (tüm bitleri 1 yap - yani tümü dolu)
  // pmm_init_region ile kullanılabilir alanları açacağız
  memset(pmm_bitmap, 0xFF, pmm_max_blocks / PMM_BLOCKS_PER_BYTE);

  kprintf("PMM: Initialized. Max blocks: %d, Bitmap at: 0x%x\n", pmm_max_blocks,
          bitmap_addr);
}

// Belirli bir bölgeyi kullanılabilir (boş) olarak işaretle
void pmm_init_region(uint32_t base, size_t size) {
  uint32_t align = base / PMM_BLOCK_SIZE;
  uint32_t blocks = size / PMM_BLOCK_SIZE;

  for (; blocks > 0; blocks--) {
    bitmap_unset(align++);
    pmm_used_blocks--;
  }

  // Sıfırıncı bloğu asla verme (NULL pointer koruması)
  bitmap_set(0);
}

// Belirli bir bölgeyi dolu (rezerve) olarak işaretle
void pmm_deinit_region(uint32_t base, size_t size) {
  uint32_t align = base / PMM_BLOCK_SIZE;
  uint32_t blocks = size / PMM_BLOCK_SIZE;

  for (; blocks > 0; blocks--) {
    bitmap_set(align++);
    pmm_used_blocks++;
  }
}

// 4KB'lık bir fiziksel blok ayır
void *pmm_alloc_block(void) {
  int free_block = bitmap_find_first_free();
  if (free_block == -1) {
    return NULL; // Bellek bitti!
  }

  bitmap_set(free_block);
  pmm_used_blocks++;

  return (void *)(free_block * PMM_BLOCK_SIZE);
}

// Bir bloğu serbest bırak
void pmm_free_block(void *p) {
  uint32_t addr = (uint32_t)p;
  uint32_t block = addr / PMM_BLOCK_SIZE;

  bitmap_unset(block);
  pmm_used_blocks--;
}

size_t pmm_get_block_count(void) { return pmm_max_blocks; }
size_t pmm_get_free_block_count(void) {
  return pmm_max_blocks - pmm_used_blocks;
}
