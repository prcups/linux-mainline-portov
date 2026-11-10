/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __MSM_DSI_PORTOV_DIAG_H__
#define __MSM_DSI_PORTOV_DIAG_H__

#include <linux/kconfig.h>

struct mipi_dsi_host;

/* Temporary portov bring-up interface, guarded for modular configurations. */
#if IS_REACHABLE(CONFIG_DRM_MSM)
void msm_dsi_host_portov_panel_trace(struct mipi_dsi_host *host,
				   unsigned int stage);
void msm_dsi_host_portov_panel_ready(struct mipi_dsi_host *host);
#else
static inline void
msm_dsi_host_portov_panel_trace(struct mipi_dsi_host *host, unsigned int stage)
{
}
static inline void msm_dsi_host_portov_panel_ready(struct mipi_dsi_host *host)
{
}
#endif

#endif /* __MSM_DSI_PORTOV_DIAG_H__ */
