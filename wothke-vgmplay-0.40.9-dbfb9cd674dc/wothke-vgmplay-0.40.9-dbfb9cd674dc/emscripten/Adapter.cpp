/*
* This file adapts "vgmplay" to the interface expected by my generic JavaScript player..
*
* Copyright (C) 2015-2023 Juergen Wothke
*
* LICENSE
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or (at
* your option) any later version. This library is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */
#include <string.h>

#include <emscripten.h>

#include "vgminterface.h"

#include "MetaInfoHelper.h"
using emsutil::MetaInfoHelper;

// see Sound::Sample::CHANNELS
#define CHANNELS 2
#define BYTES_PER_SAMPLE 2

#define TEXT_MAX	1024


namespace vgm {

	using std::string;

	class Adapter {
	public:
		Adapter() :	_bufferSize(0), _sampleBuffer(NULL), _samplesAvailable(0), _maxPos(-1), _initialized(0),
					_resourcePath(NULL), _asyncLoadNotReady(0)
		{
		}
		~Adapter()
		{
		}

		void teardown()
		{
			MetaInfoHelper::getInstance()->clear();

			switch(_initialized) {
				case 0:
					break;
				case 4:
					StopVGM();
				case 3:
					CloseVGMFile();
				case 2:
				case 1:
					VGMPlay_Deinit();
					break;
			}
			_initialized = 0;
			_asyncLoadNotReady = 0;
		}

		const char** getMeta()
		{
			MetaInfoHelper *info = MetaInfoHelper::getInstance();

			if (_initialized >= 3) {
				const wchar_t* titleTag = GetTagStrEJ(VGMTag.strTrackNameE, VGMTag.strTrackNameJ);
				const wchar_t* gameTag = GetTagStrEJ(VGMTag.strGameNameE, VGMTag.strGameNameJ);
				const wchar_t* authorTag = GetTagStrEJ(VGMTag.strAuthorNameE, VGMTag.strAuthorNameJ);
				const wchar_t* systemTag = GetTagStrEJ(VGMTag.strSystemNameE, VGMTag.strSystemNameJ);


				info->setVText(0, titleTag, "");
				info->setVText(1, authorTag, "");
				info->setVText(2, gameTag, "");
				info->setVText(3, VGMTag.strNotes, "");
				info->setVText(4, systemTag, "");
				info->setText(5, GetChipsInfo(), "");
				info->setText(6, "1", "");				// always one track per file
			}
			return info->getMeta();
		}

		void setResourcePath(char *path)
		{
			if (_resourcePath) free(_resourcePath);

			_resourcePath = (char*)malloc(strlen(path) + 1);
			strcpy(_resourcePath, path);
		}

		int loadFile(char *filename, void * inBuffer, uint32_t inBufSize, uint32_t sampleRate, uint32_t audioBufSize, uint32_t scopesEnabled)
		{
			// fixme: scopesEnabled support not implemented
			allocBuffers(audioBufSize);

			if (!_initialized)
			{
				VGMPlay_Init();
				_initialized = 1;

				snprintf(_tmpBuf, TEXT_MAX, "%s%s", _resourcePath, "VGMPlay.ini");
				ReadOptions(_tmpBuf);

				if (_asyncLoadNotReady)
				{
					return -1;
				}

				// override whatever may be hardcoded in the config.. to avoid resampling
				SampleRate = sampleRate;

				VGMPlay_Init2();
				_initialized= 2;
			}
			else
			{
				fprintf(stdout, "error: reached supposedly dead code\n");
			}

			if(!OpenVGMFile(filename))
				return -1;

			_initialized= 3;

			return 0;
		}

		void setBoost(int boost)
		{
			BoostVolume = boost;	// hack to force louder playback
		}

		int setSubsong(int subsong)
		{
			PlayVGM();	// this sets up the chips (and loads yrw801.rom)

			if (_asyncLoadNotReady)
			{
				return -1;
			}
			_initialized = 4;

			_maxPos = -1;

			return 0;
		}

		int genSamples()
		{
			if (_initialized < 4) return 0;	// just in case

			if (!EndPlay)
			{
				int size = FillBuffer(_sampleBuffer, _bufferSize);
				if (size)
				{
					_samplesAvailable = size;
					return 0;	// we might want to handle errors (-1) separately..
				}
			}

			StopVGM();
			return 1;
		}

		char* getSampleBuffer()
		{
			return (char*)_sampleBuffer;
		}

		int getSampleBufferLength()
		{
			return _samplesAvailable;
		}

		int getCurrentPosition()
		{
			return ((long long)SamplePlayback2VGM(PlayingTime)) * 1000 / VGMSampleRate;
		}

		int getMaxPosition()
		{
			if (_initialized < 3) return -1;

			if (_maxPos<0)
			{
				INT32 l= GetFileLength(&VGMHead);
				_maxPos= SampleVGM2Playback(l);
			}
			// original vglplay seems to measure in samples

			return ((long long)_maxPos) * 1000 / VGMSampleRate;
		}

		int seekPosition(int pos)
		{
			if (_initialized < 3) return -1;

			if (pos > getMaxPosition())
			{
				return -1;
			}
			SeekVGM(false, ((long long)pos) * VGMSampleRate / 1000);
			return 0;
		}

		const char *getResourcePath()
		{
			return _resourcePath;
		}

		void setAsyncLoadNotReady()
		{
			_asyncLoadNotReady = 1;
		}
	private:

		int GetFileLength(VGM_HEADER* FileHead)
		{
			UINT32 SmplCnt;

			if (! VGMMaxLoopM && FileHead->lngLoopSamples)
				return -1000;

			// Note: SmplCnt is ALWAYS 44.1 KHz, VGM's native sample rate
			SmplCnt = FileHead->lngTotalSamples + FileHead->lngLoopSamples * (VGMMaxLoopM - 1);

			INT32 fadeMSec = (FileHead->lngLoopSamples) ? FadeTime + PauseTime : PauseTime;

			return SmplCnt + (fadeMSec / 1000 * VGMSampleRate);
		}

		void allocBuffers(int size)
		{
			if (size > _bufferSize)
			{
				if (_sampleBuffer) free(_sampleBuffer);

				_sampleBuffer = (WAVE_16BS*)malloc(sizeof(WAVE_16BS) * size);
				_bufferSize = size;
			}
		}
	private:
		int _bufferSize;
		WAVE_16BS* _sampleBuffer;
		int _samplesAvailable;
		INT32 _maxPos;
		char _initialized;
		char *_resourcePath;
		int _asyncLoadNotReady;
		char _tmpBuf[TEXT_MAX];
	};
};

vgm::Adapter _adapter;


extern "C" const char *getResourcePath() {
	return _adapter.getResourcePath();
}

extern "C" void setAsyncLoadNotReady() {
	_adapter.setAsyncLoadNotReady();
}




// old style EMSCRIPTEN C function export to JavaScript.
// todo: code might be cleaned up using EMSCRIPTEN's "new" Embind feature:
// https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html
#define EMBIND(retType, func)  \
	extern "C" retType func __attribute__((noinline)); \
	extern "C" retType EMSCRIPTEN_KEEPALIVE func


EMBIND(int, emu_load_file(char *filename, void * inBuffer, uint32_t inBufSize, uint32_t sampleRate, uint32_t audioBufSize, uint32_t scopesEnabled)) {
	return _adapter.loadFile(filename, inBuffer, inBufSize, sampleRate, audioBufSize, scopesEnabled); }
EMBIND(void, emu_teardown())						{ _adapter.teardown(); }
EMBIND(int, emu_get_sample_rate())					{ return SampleRate;}
EMBIND(int, emu_set_subsong(int track)){ return _adapter.setSubsong(track); }
EMBIND(const char**, emu_get_track_info())			{ return _adapter.getMeta(); }
EMBIND(char*, emu_get_audio_buffer())				{ return _adapter.getSampleBuffer(); }
EMBIND(int, emu_get_audio_buffer_length())			{ return _adapter.getSampleBufferLength(); }
EMBIND(int, emu_compute_audio_samples())			{ return _adapter.genSamples(); }
EMBIND(int, emu_get_current_position())						{ return _adapter.getCurrentPosition(); }
EMBIND(void, emu_seek_position(int frames))			{ _adapter.seekPosition(frames); }
EMBIND(int, emu_get_max_position())					{ return _adapter.getMaxPosition(); }

// --- add-on
EMBIND(void, emu_set_resource_path(char *resPath))	{ return _adapter.setResourcePath(resPath); }
EMBIND(void, emu_set_boost(int boost))				{ return _adapter.setBoost(boost); }

