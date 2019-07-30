/* log.h
 * - logging function declarations for ices
 * Copyright (c) 2000 Alexander Haväng
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/* Public function declarations */
void ices_log(const char *fmt, ...);
void ices_log_error(const char *fmt, ...);
void ices_log_debug(const char *fmt, ...);
void ices_log_error_output(const char *fmt, ...);
char *ices_log_get_error(void);
int ices_log_reopen_logfile(void);
void ices_log_initialize(void);
void ices_log_shutdown(void);
void ices_log_daemonize(void);

















