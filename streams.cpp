/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "streams.h"
#include "logger.h"
#include <cstdlib>
#include <cstring>
#include <assert.h>

using namespace std;

sync_stream::sync_stream():head(0),tail(0),buf_size(1024*1024)
{
	printf("syn stream\n");
	buffer=new char[buf_size];
	sem_init(&mutex,0,1);
	sem_init(&notfull,0,0);
	sem_init(&ready,0,0);
}

int sync_stream::provideBuffer(int limit)
{
	sem_wait(&mutex);
	if(tail==head)
	{
		sem_post(&mutex);
		sem_wait(&ready);
	}

	bool signal=false;
	if(((tail-head+buf_size)%buf_size)==buf_size-1)
		signal=true;

	int available=(tail-head+buf_size)%buf_size;
	available=min(available,limit);
	if(head+available>buf_size)
	{
		int i=buf_size-head;
		memcpy(in_buf,buffer+head,i);
		memcpy(in_buf+i,buffer,available-i);
	}
	else
		memcpy(in_buf,buffer+head,available);

	head+=available;
	head%=buf_size;
	if(signal)
		sem_post(&notfull);
	else
		sem_post(&mutex);
	return available;
}

int sync_stream::write(char* buf, int len)
{
	sem_wait(&mutex);
	if(((tail-head+buf_size)%buf_size)==buf_size-1)
	{
		sem_post(&mutex);
		sem_wait(&notfull);
	}
	bool signal=false;
	if(tail==head)
		signal=true;

	if((head-tail+buf_size-1)%buf_size<len)
	{
		len=(head-tail+buf_size-1)%buf_size;
	}
	if(tail+len>buf_size)
	{
		int i=buf_size-tail;
		memcpy(buffer+tail,buf,i);
		memcpy(buffer,buf+i,len-i);
	}
	else
		memcpy(buffer+tail,buf,len);
	tail+=len;
	tail%=buf_size;
	if(signal)
		sem_post(&ready);
	else
		sem_post(&mutex);
	return len;
}

zlib_filter::zlib_filter():consumed(0),available(0)
{
}

void zlib_filter::initialize()
{
	//Should check that this is called only once
	available=provideBuffer(8);
	assert(available==8);
	//We read only the first 8 bytes, as those are always uncompressed

	//Now check the signature
	if(in_buf[1]!='W' || in_buf[2]!='S')
		abort();
	if(in_buf[0]=='F')
		compressed=false;
	else if(in_buf[0]=='C')
	{
		compressed=true;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		int ret = inflateInit(&strm);
		if (ret != Z_OK)
		{
			LOG(LOG_ERROR,"Failed to initialize ZLib");
			abort();
		}
	}
	else
		abort();

	//Ok, it seems to be a valid SWF, from now, if the file is compressed, data has to be inflated

	//Copy the in_buf to the out buffer
	memcpy(buffer,in_buf,8);
	setg(buffer,buffer,buffer+available);
}

zlib_filter::int_type zlib_filter::underflow()
{
	assert(gptr()==egptr());

	//First of all we add the lenght of the buffer to the consumed variable
	consumed+=(gptr()-eback());

	//The first time
	if(consumed==0)
	{
		initialize();
		return (unsigned char)buffer[0];
	}

	if(!compressed)
	{
		int real_count=provideBuffer(4096);
		assert(real_count>0);
		memcpy(buffer,in_buf,real_count);
		setg(buffer,buffer,buffer+real_count);
	}
	else
	{
		// run inflate() on input until output buffer not full
		strm.avail_out = 4096;
		strm.next_out = (unsigned char*)buffer;
		inflate(&strm, Z_NO_FLUSH);
		available=4096;
		//check if output full and wrap around
		while(strm.avail_out!=0)
		{
			int real_count=provideBuffer(4096);
			assert(strm.avail_in==0);
			if(real_count==0)
			{
				//File is not big enough to fill the buffer
				available=4096-strm.avail_out;
				break;
			}
			strm.next_in=(unsigned char*)in_buf;
			strm.avail_in=real_count;
			int ret=inflate(&strm, Z_NO_FLUSH);
			if(ret==Z_OK);
			else if(ret==Z_STREAM_END)
			{
				//The stream ended, close the buffer here
				available=4096-strm.avail_out;
				break;
			}
			else
				abort();
		}
		setg(buffer,buffer,buffer+available);
	}

	//Cast to signed, otherwise 0xff would become eof
	return (unsigned char)buffer[0];
}

zlib_filter::pos_type zlib_filter::seekoff(off_type off, ios_base::seekdir dir,ios_base::openmode mode)
{
	assert(off==0);
	//The current offset is the amount of byte completely consumed plus the amount used in the buffer
	int ret=consumed+(gptr()-eback());
	return ret;
}

zlib_file_filter::zlib_file_filter(const char* file_name)
{
	f=fopen(file_name,"rb");
	assert(f!=NULL);
}

int zlib_file_filter::provideBuffer(int limit)
{
	return fread(in_buf,1,limit,f);
}

zlib_bytes_filter::zlib_bytes_filter(const uint8_t* b, int l):buf(b),offset(0),len(l)
{
}

int zlib_bytes_filter::provideBuffer(int limit)
{
	int ret=min(limit,len-offset);
	memcpy(in_buf,buf+offset,ret);
	offset+=ret;
	return ret;
}

