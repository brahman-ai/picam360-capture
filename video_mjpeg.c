/*
 Copyright (c) 2012, Broadcom Europe Ltd
 Copyright (c) 2012, OtherCrashOverride
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the copyright holder nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Video decode demo using OpenMAX IL though the ilcient helper library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "bcm_host.h"
#include "ilclient.h"

static OMX_BUFFERHEADERTYPE* eglBuffer[2] = { };
static COMPONENT_T* egl_render[2] = { };

static void* eglImage[2] = { };

static void my_fill_buffer_done(void* data, COMPONENT_T* comp) {
	int index = (int) data;

	if (OMX_FillThisBuffer(ilclient_get_handle(egl_render[index]),
			eglBuffer[index]) != OMX_ErrorNone) {
		printf("test  OMX_FillThisBuffer failed in callback\n");
		exit(1);
	}
}

// Modified function prototype to work with pthreads
void *video_mjpeg_decode(void* arg) {
	int index;

	index = (int) ((void**) arg)[0];
	eglImage[index] = ((void**) arg)[1];

	if (eglImage[index] == 0) {
		printf("eglImage is null.\n");
		exit(1);
	}

	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	COMPONENT_T *video_decode = NULL;
	COMPONENT_T *list[3];
	TUNNEL_T tunnel[2];
	ILCLIENT_T *client;
	int status = 0;
	unsigned int data_len = 0;

	memset(list, 0, sizeof(list));
	memset(tunnel, 0, sizeof(tunnel));

	if ((client = ilclient_init()) == NULL) {
		return (void *) -3;
	}

	if (OMX_Init() != OMX_ErrorNone) {
		ilclient_destroy(client);
		return (void *) -4;
	}

	// callback
	ilclient_set_fill_buffer_done_callback(client, my_fill_buffer_done,
			(void*) index);

	// create video_decode
	if (ilclient_create_component(client, &video_decode, "video_decode",
			ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0)
		status = -14;
	list[0] = video_decode;

	// create egl_render
	if (status == 0
			&& ilclient_create_component(client, &egl_render[index],
					"egl_render",
					ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_OUTPUT_BUFFERS)
					!= 0)
		status = -14;
	list[1] = egl_render[index];

	set_tunnel(tunnel, video_decode, 131, egl_render[index], 220);

	if (status == 0)
		ilclient_change_component_state(video_decode, OMX_StateIdle);

	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = 130;
	format.eCompressionFormat = OMX_VIDEO_CodingMJPEG;

	char buff[256];
	sprintf(buff, "cam%d", index);
	int descriptor = open(buff, O_RDONLY);
	if (descriptor == -1) {
		printf("failed to open %s\n", buff);
		exit(-1);
	}

	if (status == 0
			&& OMX_SetParameter(ILC_GET_HANDLE(video_decode),
					OMX_IndexParamVideoPortFormat, &format) == OMX_ErrorNone
			&& ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL)
					== 0) {
		OMX_BUFFERHEADERTYPE *buf;
		int port_settings_changed = 0;
		int first_packet = 1;
		int marker = 0;
		int left_len = 0;
		char *left_data = NULL;

		ilclient_change_component_state(video_decode, OMX_StateExecuting);

		printf("milestone\n");

		while ((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL) {

			int eoi = 0;
			// feed data and wait until we get port settings changed
			unsigned char *dest = buf->pBuffer;
			if (left_len != 0) {
				memcpy(dest, left_data, left_len);
				free(left_data);
				data_len = left_len;
				left_len = 0;
			} else {
				data_len = read(descriptor, dest, buf->nAllocLen);
			}
			for (int i = 0; i < data_len; i++) {
				if (marker) {
					marker = 0;
					if (buf->pBuffer[i] == 0xd9) { //EOI
						eoi = 1;
						left_len = data_len - (i + 1);
						data_len = i + 1;
						left_data = malloc(left_len);
						memcpy(left_data, dest + data_len, left_len);
						break;
					}
				} else if (buf->pBuffer[i] == 0xff) {
					marker = 1;
				}
			}

			if (port_settings_changed == 0
					&& ((data_len > 0
							&& ilclient_remove_event(video_decode,
									OMX_EventPortSettingsChanged, 131, 0, 0, 1)
									== 0)
							|| (data_len == 0
									&& ilclient_wait_for_event(video_decode,
											OMX_EventPortSettingsChanged, 131,
											0, 0, 1,
											ILCLIENT_EVENT_ERROR
													| ILCLIENT_PARAMETER_CHANGED,
											10000) == 0))) {
				port_settings_changed = 1;

				if (ilclient_setup_tunnel(tunnel, 0, 0) != 0) {
					status = -7;
					break;
				}

				// Set egl_render to idle
				ilclient_change_component_state(egl_render[index],
						OMX_StateIdle);

				// Enable the output port and tell egl_render to use the texture as a buffer
				//ilclient_enable_port(egl_render, 221); THIS BLOCKS SO CAN'T BE USED
				if (OMX_SendCommand(ILC_GET_HANDLE(egl_render[index]),
						OMX_CommandPortEnable, 221, NULL) != OMX_ErrorNone) {
					printf("OMX_CommandPortEnable failed.\n");
					exit(1);
				}

				if (OMX_UseEGLImage(ILC_GET_HANDLE(egl_render[index]),
						&eglBuffer[index], 221, NULL, eglImage[index])
						!= OMX_ErrorNone) {
					printf("OMX_UseEGLImage failed.\n");
					exit(1);
				}

				// Set egl_render to executing
				ilclient_change_component_state(egl_render[index],
						OMX_StateExecuting);

				// Request egl_render to write data to the texture buffer
				if (OMX_FillThisBuffer(ILC_GET_HANDLE(egl_render[index]),
						eglBuffer[index]) != OMX_ErrorNone) {
					printf("OMX_FillThisBuffer failed.\n");
					exit(1);
				}
			}
			if (!data_len) {
				break;
			}
			buf->nFilledLen = data_len;

			buf->nOffset = 0;
			if (first_packet) {
				buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
				first_packet = 0;
			} else {
				buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
			}

			if (eoi) {
				buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
			}

			if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf)
					!= OMX_ErrorNone) {
				status = -6;
				break;
			}
		}

		buf->nFilledLen = 0;
		buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

		if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf)
				!= OMX_ErrorNone)
			status = -20;

		// need to flush the renderer to allow video_decode to disable its input port
		ilclient_flush_tunnels(tunnel, 0);

		ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
	}

	ilclient_disable_tunnel(tunnel);
	ilclient_teardown_tunnels(tunnel);

	ilclient_state_transition(list, OMX_StateIdle);
	ilclient_state_transition(list, OMX_StateLoaded);

	ilclient_cleanup_components(list);

	OMX_Deinit();

	ilclient_destroy(client);
	return (void *) status;
}

