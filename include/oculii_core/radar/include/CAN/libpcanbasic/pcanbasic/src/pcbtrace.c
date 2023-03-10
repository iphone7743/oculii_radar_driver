/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * @file pcbtrace.c
 * @brief Tools to trace CAN message within PCANBasic API.
 *
 * $Id: pcbtrace.c 6126 2020-03-04 16:00:13Z Stephane $
 *
 * Copyright (C) 2001-2020  PEAK System-Technik GmbH <www.peak-system.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * PCAN is a registered Trademark of PEAK-System Germany GmbH
 *
 * Contact:      <linux@peak-system.com>
 * Maintainer:   Fabrice Vergnaud <f.vergnaud@peak-system.com>
 */

#include "pcbtrace.h"
#include "pcaninfo.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>



#define PCBTRACE_MAX_MSG	600

static char * pcbtrace_hw_to_string(enum pcaninfo_hw hw);
static int pcbtrace_write_header(struct pcbtrace_ctx *ctx, enum pcbtrace_version version);
static void pcbtrace_size_check(struct pcbtrace_ctx *ctx);
static void pcbtrace_open_init(struct pcbtrace_ctx *ctx, enum pcaninfo_hw hw, uint ch_idx);
static int pcbtrace_open_next(struct pcbtrace_ctx *ctx);
static const char* pcbtrace_get_type(TPCANMsgFD *msg);
static const char* pcbtrace_get_canid(TPCANMsgFD *msg, char* buf, size_t size, int *noid);

/* PRIVATE FUNCTIONS */
char * pcbtrace_hw_to_string(enum pcaninfo_hw hw) {
	char * res;

	res = "";
	switch (hw) {
	case PCANINFO_HW_DNG:
		res = "PCAN_DNGBUS";
		break;
	case PCANINFO_HW_ISA:
		res = "PCAN_ISABUS";
		break;
	case PCANINFO_HW_LAN:
		res = "PCAN_LANBUS";
		break;
	case PCANINFO_HW_PCC:
		res = "PCAN_PCCBUS";
		break;
	case PCANINFO_HW_PCI:
		res = "PCAN_PCIBUS";
		break;
	case PCANINFO_HW_PEAKCAN:
		res = "PCAN_CANBUS";
		break;
	case PCANINFO_HW_USB:
		res = "PCAN_USBBUS";
		break;
	case PCANINFO_HW_VIRTUAL:
		res = "PCAN_VIRTUALBUS";
		break;
	case PCANINFO_HW_NONE:
		res = "PCAN_NONEBUS";
		break;
	default:
		res = "UNKNOWN";
		break;
	}
	return res;
}

int pcbtrace_write_header(struct pcbtrace_ctx *ctx, enum pcbtrace_version version) {
	char buf[PCBTRACE_MAX_MSG];
	char tmp[PCANINFO_MAX_CHAR_SIZE];
	size_t n, ntmp;
	time_t traw;
	struct tm *t;

	if (ctx == NULL || !ctx->status || !ctx->pfile)
		return EINVAL;

	/* file version */
	switch (version) {
	case V1_1:
		snprintf(buf, PCBTRACE_MAX_MSG, ";$FILEVERSION=1.1\n");
		break;
	case V2_0:
	default:
		snprintf(buf, PCBTRACE_MAX_MSG, ";$FILEVERSION=2.0\n");
		break;
	}
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	/* starttime */
	time(&traw);
	t = localtime(&traw);
	gettimeofday(&ctx->time_start, NULL);
	snprintf(buf, PCBTRACE_MAX_MSG, ";$STARTTIME=%lu.%lu\n", ctx->time_start.tv_sec, 0UL); // ctx->time_start.tv_usec);
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	/* columns */
	switch (version) {
	case V1_1:
		// not supported
		break;
	case V2_0:
	default:
		snprintf(buf, PCBTRACE_MAX_MSG, ";$COLUMNS=N,O,T,I,d,L,D\n");
		n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
		break;
	}
	/* separator */
	snprintf(buf, PCBTRACE_MAX_MSG, ";\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;

	/* file path */
	snprintf(buf, PCBTRACE_MAX_MSG, ";   %s\n", ctx->filename);
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	/* start time */
	snprintf(buf, PCBTRACE_MAX_MSG, ";   Start time: %.2d/%.2d/%.4d %.2d:%.2d:%.2d.000.0\n",
		t->tm_mday, t->tm_mon, t->tm_year,t->tm_hour,t->tm_min,t->tm_sec);
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	/* generated by */
	snprintf(buf, PCBTRACE_MAX_MSG, ";   Generated by PCAN-Basic API\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";-------------------------------------------------------------------------------\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;

	/* connection information */
	if (ctx->pinfo != NULL) {
		snprintf(buf, PCBTRACE_MAX_MSG, ";   Connection                                Bit rate\n");
		n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
		/* channel info */
		snprintf(buf, PCBTRACE_MAX_MSG, ";   %s (%s)           ", ctx->chname, ctx->pinfo->path);
		ntmp = strlen(buf);
		n = fwrite(buf, ntmp, sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
		/* init string */
		snprintf(buf, PCBTRACE_MAX_MSG, "%s\n", 
			pcaninfo_bitrate_to_init_string(ctx->pinfo, tmp, sizeof(tmp)));
		n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
		/* human-readable bitrate */
		memset(buf, ' ', ntmp);
		buf[0] = ';';
		buf[ntmp] = 0;
		n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
		snprintf(buf, PCBTRACE_MAX_MSG, "%s\n", 
			pcaninfo_bitrate_to_string(ctx->pinfo, tmp, sizeof(tmp)));
		n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
		snprintf(buf, PCBTRACE_MAX_MSG, ";-------------------------------------------------------------------------------\n");
		n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
		if (n <= 0)
			return errno;
	}

	/* connection information */
	snprintf(buf, PCBTRACE_MAX_MSG, "; Glossary:\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";   Direction of Message:\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     Rx: The frame was received\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     Tx: The frame was transmitted\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";   Type of message:\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     DT: CAN or J1939 data frame\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     FD: CAN FD data frame\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     FB: CAN FD data frame with BRS bit set (Bit Rate Switch)\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     FE: CAN FD data frame with ESI bit set (Error State Indicator)\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     BI: CAN FD data frame with both BRS and ESI bits set\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     RR: Remote Request Frame\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     ST: Hardware Status change\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     ER: Error Frame\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";     EV: Event. User-defined text, begins directly after 2-digit type indicator\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";-------------------------------------------------------------------------------\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";   Message   Time    Type ID     Rx/Tx\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";   Number    Offset  |    [hex]  |  Data Length Code\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";   |         [ms]    |    |      |  |  Data [hex]\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";   |         |       |    |      |  |  | \n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	snprintf(buf, PCBTRACE_MAX_MSG, ";---+-- ------+------ +- --+----- +- +- +- -- -- -- -- -- -- --\n");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;

	return 0;
}

void pcbtrace_size_check(struct pcbtrace_ctx *ctx) {
	struct stat st;

	stat(ctx->filename, &st);
	if (st.st_size > ctx->maxsize * 1000000) {
		pcbtrace_close(ctx);
		if (ctx->flags & TRACE_FILE_SEGMENTED)
			pcbtrace_open_next(ctx);
		else
			ctx->status = PCAN_PARAMETER_OFF;
	}
}

void pcbtrace_open_init(struct pcbtrace_ctx *ctx, enum pcaninfo_hw hw, uint ch_idx) {
	char filename[100];
	char strtmp[9];
	char *str;
	size_t len;
	time_t traw;
	struct tm *t;

	/* check trace is already closed */
	pcbtrace_close(ctx);
	/* build filename based on time and channel */
	time(&traw);
	t = localtime(&traw);
	filename[0] = '\0';
	str = "";
	if (ctx->flags & TRACE_FILE_DATE) {
		len = snprintf(strtmp, sizeof(strtmp), "%04u%02u%02u", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday);
		strncat(filename, strtmp, len);
		str = "_";
	}
	if (ctx->flags & TRACE_FILE_TIME) {
		len = snprintf(strtmp, sizeof(strtmp), "%02u%02u%02u", t->tm_hour, t->tm_min, t->tm_sec);
		strncat(filename, strtmp, len);
		str = "_";
	}
	snprintf(ctx->chname, sizeof(ctx->chname), "%s%d", pcbtrace_hw_to_string(hw), ch_idx);
	snprintf(ctx->filename_chunk, sizeof(ctx->filename_chunk), "%s%s%s", filename, str, ctx->chname);
	/* remove trailing '/' in location */
	len = strnlen(ctx->directory, sizeof(ctx->directory));
	if (len > 2 && ctx->directory[len - 1] == 0 &&
			ctx->directory[len - 2] == '/')
		ctx->directory[len - 2] = 0;
}

int pcbtrace_open_next(struct pcbtrace_ctx *ctx) {
	ctx->idx++;
	snprintf(ctx->filename, sizeof(ctx->filename), "%s/%s_%02d.trc", ctx->directory, ctx->filename_chunk, ctx->idx);
	/* open file and update context */
	ctx->pfile = fopen(ctx->filename, "w");
	if (ctx->pfile)
		return pcbtrace_write_header(ctx, V2_0);
	return errno;
}

const char* pcbtrace_get_type(TPCANMsgFD *msg) {
	char* result;

	/* check error frame */
	if ((msg->MSGTYPE & PCAN_MESSAGE_ERRFRAME) == PCAN_MESSAGE_ERRFRAME) {
		result = "ER";
	}
	/* check status frame */
	else if ((msg->MSGTYPE & PCAN_MESSAGE_STATUS) == PCAN_MESSAGE_STATUS) {
		result = "ST";
	}
	else {	/* msg is valid */
			/* check CAN FD */
		if ((msg->MSGTYPE & PCAN_MESSAGE_FD) == PCAN_MESSAGE_FD) {
			/* check BRS & ESI */
			if ((msg->MSGTYPE & PCAN_MESSAGE_BRS) == PCAN_MESSAGE_BRS &&
				(msg->MSGTYPE & PCAN_MESSAGE_ESI) == PCAN_MESSAGE_ESI) {
				result = "BI";
			}
			/* check BRS */
			else if ((msg->MSGTYPE & PCAN_MESSAGE_BRS) == PCAN_MESSAGE_BRS) {
				result = "FB";
			}
			/* check ESI */
			else if ((msg->MSGTYPE & PCAN_MESSAGE_ESI) == PCAN_MESSAGE_ESI) {
				result = "FE";
			}
			else {
				/* classic CAN FD frame */
				result = "FD";
			}
		}
		else { /* CAN frame */
			   /* check RTR */
			if ((msg->MSGTYPE & PCAN_MESSAGE_RTR) == PCAN_MESSAGE_RTR) {
				result = "RR";
			}
			else {
				/* classic CAN frame */
				result = "DT";
			}
		}
	}
	return result;
}

const char* pcbtrace_get_canid(TPCANMsgFD *msg, char* buf, size_t size, int *has_no_canid) {
	char *ptr = buf;
	
	*has_no_canid = ((msg->MSGTYPE & PCAN_MESSAGE_ERRFRAME) == PCAN_MESSAGE_ERRFRAME ||
		(msg->MSGTYPE & PCAN_MESSAGE_STATUS) == PCAN_MESSAGE_STATUS);

	if (*has_no_canid) {
		// no CAN ID displayed
		snprintf(buf, size, "        ");
	}
	else {
		/* format CAN ID as hex */
		if ((msg->MSGTYPE & PCAN_MESSAGE_EXTENDED) == PCAN_MESSAGE_EXTENDED)
			snprintf(buf, size, "%08x", msg->ID);
		else
			snprintf(buf, size, "    %04x", msg->ID);
		/* format to upper chars */
		while (*ptr) {
			*ptr = toupper(*ptr);
			ptr++;
		}
	}
	return buf;
}

/* PUBLIC FUNCTIONS */
void pcbtrace_set_defaults(struct pcbtrace_ctx *ctx) {
	if (ctx == NULL)
		return;
	sprintf(ctx->directory, ".");
	ctx->flags = 0;
	ctx->maxsize = 10;
	ctx->status = PCAN_PARAMETER_OFF;
	ctx->pinfo = NULL;
}

int pcbtrace_open(struct pcbtrace_ctx *ctx, enum pcaninfo_hw hw, uint ch_idx) {
	if (ctx == NULL)
		return EINVAL;
	pcbtrace_open_init(ctx, hw, ch_idx);
	ctx->idx = 0;
	ctx->msg_cnt = 0;
	return pcbtrace_open_next(ctx);
}

int pcbtrace_close(struct pcbtrace_ctx *ctx) {
	if (ctx == NULL)
		return EINVAL;
	if (ctx->pfile != NULL) {
		fclose(ctx->pfile);
		ctx->pfile = NULL;
	}
	return 0;
}

int pcbtrace_write_msg(struct pcbtrace_ctx *ctx, TPCANMsgFD *msg, int data_len, struct timeval *tv, int rx) {
	char buf[PCBTRACE_MAX_MSG];
	char * str;
	int i, n, len;
	int has_no_canid;

	if (ctx == NULL)
		return EINVAL;
	if (!ctx->status)
		return 0;
	if (ctx->pfile == NULL)
		return EBADF;
		
	ctx->msg_cnt++;
	len = 0;

	/* write: "Frame_Nb   Time_msec.micros   Direction   " */
	snprintf(buf, PCBTRACE_MAX_MSG, "%7lu   %7lu.%03lu ",
		ctx->msg_cnt, (tv->tv_sec - ctx->time_start.tv_sec) * 1000 + (tv->tv_usec / 1000), (tv->tv_usec % 1000));
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	len += n;
	/* write: "Type " */
	snprintf(buf, PCBTRACE_MAX_MSG, "%s ", pcbtrace_get_type(msg));
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	len += n;
	/* write: "ID" */
	pcbtrace_get_canid(msg, buf, PCBTRACE_MAX_CHAR_SIZE, &has_no_canid);
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	/* write: " Rx/Tx " */
	snprintf(buf, PCBTRACE_MAX_MSG, " %s ", rx ? "Rx" : "Tx");
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	/* write: "DLC " */
	if (has_no_canid)
		snprintf(buf, PCBTRACE_MAX_MSG, "   ");
	else 
		snprintf(buf, PCBTRACE_MAX_MSG, "%-2d ", msg->DLC);
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	len += n;
	/* prepare data buffer: depends on msg's type */
	if ((msg->MSGTYPE & PCAN_MESSAGE_STATUS) == PCAN_MESSAGE_STATUS) {
		/* write: state info */
		str = "";
		if(msg->DATA[3] & PCAN_ERROR_BUSLIGHT)
			str = "BUSLIGHT";
		if(msg->DATA[3] & PCAN_ERROR_BUSHEAVY)
			str = "BUSHEAVY";
		if(msg->DATA[3] & PCAN_ERROR_BUSOFF)
			str = "BUSOFF";
		snprintf(buf, PCBTRACE_MAX_MSG, "%02x %02x %02x %02x (%s)",
					msg->DATA[0], msg->DATA[1], msg->DATA[2],
					msg->DATA[3], str);
	}
	else if ((msg->MSGTYPE & PCAN_MESSAGE_ERRFRAME) == PCAN_MESSAGE_ERRFRAME) {
		/* write: error info */
		switch (msg->DATA[0]) {
		case 1:
			str = "Bit Error";
			break;
		case 2:
			str = "Form Error";
			break;
		case 4:
			str = "Stuff Error";
			break;
		case 8:
			str = "Other Error";
			break;
		default:
			str = "Unknown Error";
			break;
		}
		snprintf(buf, PCBTRACE_MAX_MSG, "%02x %02x %02x %02x %02x (%s)",
			msg->DATA[0], msg->DATA[1], msg->DATA[2],
			msg->DATA[3], msg->DATA[4], str);
	}
	else {
		/* write: DATA */
		if(msg->MSGTYPE & PCAN_MESSAGE_RTR)
			snprintf(buf, PCBTRACE_MAX_MSG, "RTR");
		else {
			char bufData[10];
			if (data_len > 0) {
				buf[0] = 0;
				for (i = 0; i < data_len; i++)
				{
					snprintf(bufData, 10, "%.2X ", msg->DATA[i]);
					strncat(buf, bufData, PCBTRACE_MAX_MSG - strlen(buf));
				}
			}
			else {
				buf[0] = ' ';
				buf[1] = 0;
			}
		}
	}
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	len += n;
	/* write EOL */
	buf[0] = '\n';
	buf[1] = '\0';
	n = fwrite(buf, strlen(buf), sizeof(char), ctx->pfile);
	if (n <= 0)
		return errno;
	len += n;
	pcbtrace_size_check(ctx);
	return len;
}

int pcbtrace_write(struct pcbtrace_ctx *ctx, const char * buffer, uint size) {
	int n;

	if (ctx == NULL)
		return EINVAL;
	if (!ctx->status)
		return 0;
	if (ctx->pfile == NULL)
		return EBADF;

	n = fwrite(buffer, size, 1, ctx->pfile);
	if (n <= 0)
		return errno;
	pcbtrace_size_check(ctx);
	return n;
}



