#ifndef AIO_STUB_DRIVER_SPI_MASTER_H
#define AIO_STUB_DRIVER_SPI_MASTER_H

// Minimal ESP-IDF SPI master shim. media_player references the bus
// type but the harness never drives real SPI.

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int spi_host_device_t;
#define HSPI_HOST 1
#define VSPI_HOST 2

typedef struct { int dummy; } spi_bus_config_t;
typedef struct { int dummy; } spi_device_interface_config_t;
typedef struct spi_device_t *spi_device_handle_t;
typedef struct {
    size_t length;
    size_t rxlength;
    void *user;
    const void *tx_buffer;
    void *rx_buffer;
} spi_transaction_t;

static inline int spi_bus_initialize(spi_host_device_t, const spi_bus_config_t *, int) { return 0; }
static inline int spi_bus_free(spi_host_device_t) { return 0; }
static inline int spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t *, spi_device_handle_t *) { return 0; }
static inline int spi_bus_remove_device(spi_device_handle_t) { return 0; }
static inline int spi_device_polling_transmit(spi_device_handle_t, spi_transaction_t *) { return 0; }
static inline int spi_device_transmit(spi_device_handle_t, spi_transaction_t *) { return 0; }

#ifdef __cplusplus
}
#endif

#endif
