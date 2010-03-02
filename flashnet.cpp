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

#include "abc.h"
#include "flashnet.h"
#include "class.h"
#include <curl/curl.h>

using namespace std;
using namespace lightspark;

REGISTER_CLASS_NAME(URLLoader);
REGISTER_CLASS_NAME(URLLoaderDataFormat);
REGISTER_CLASS_NAME(URLRequest);
REGISTER_CLASS_NAME(URLVariables);
REGISTER_CLASS_NAME(SharedObject);
REGISTER_CLASS_NAME(ObjectEncoding);
REGISTER_CLASS_NAME(NetConnection);
REGISTER_CLASS_NAME(NetStream);

URLRequest::URLRequest()
{
}

void URLRequest::sinit(Class_base* c)
{
	assert(c->constructor==NULL);
	c->constructor=new Function(_constructor);
}

ASFUNCTIONBODY(URLRequest,_constructor)
{
	URLRequest* th=static_cast<URLRequest*>(obj->implementation);
	if(argslen>0 && args[0]->getObjectType()==T_STRING)
	{
		th->url=args[0]->toString();
		cout << "url is " << th->url << endl;
	}
	obj->setSetterByQName("url","",new Function(_setUrl));
	obj->setGetterByQName("url","",new Function(_getUrl));
	return NULL;
}

ASFUNCTIONBODY(URLRequest,_setUrl)
{
	URLRequest* th=static_cast<URLRequest*>(obj->implementation);
	th->url=args[0]->toString();
	cout << "Setting url to " << th->url << endl;
	return NULL;
}

ASFUNCTIONBODY(URLRequest,_getUrl)
{
	URLRequest* th=static_cast<URLRequest*>(obj->implementation);
	return Class<ASString>::getInstanceS(true,th->url)->obj;
}

URLLoader::URLLoader():dataFormat("text"),data(NULL)
{
}

void URLLoader::sinit(Class_base* c)
{
	assert(c->constructor==NULL);
	c->constructor=new Function(_constructor);
	c->super=Class<EventDispatcher>::getClass();
	c->max_level=c->super->max_level+1;
}

void URLLoader::buildTraits(ASObject* o)
{
	o->setGetterByQName("dataFormat","",new Function(_getDataFormat));
	o->setGetterByQName("data","",new Function(_getData));
	o->setSetterByQName("dataFormat","",new Function(_setDataFormat));
	o->setVariableByQName("load","",new Function(load));
}

ASFUNCTIONBODY(URLLoader,_constructor)
{
	EventDispatcher::_constructor(obj,NULL,0);
	return NULL;
}

ASFUNCTIONBODY(URLLoader,load)
{
	URLLoader* th=static_cast<URLLoader*>(obj->implementation);
	ASObject* arg=args[0];
	assert(arg->prototype==Class<URLRequest>::getClass());
	URLRequest* urlRequest=static_cast<URLRequest*>(arg->implementation);
	th->url=urlRequest->url;
	ASObject* data=arg->getVariableByQName("data","").obj;
	if(data)
	{
		if(data->prototype==Class<URLVariables>::getClass())
			abort();
		else
		{
			const tiny_string& tmp=data->toString();
			//TODO: Url encode the string
			string tmp2;
			tmp2.reserve(tmp.len()*2);
			for(int i=0;i<tmp.len();i++)
			{
				if(tmp[i]==' ')
				{
					char buf[4];
					sprintf(buf,"%%%x",(unsigned char)tmp[i]);
					tmp2+=buf;
				}
				else
					tmp2.push_back(tmp[i]);
			}
			th->url+=tmp2.c_str();
		}
	}
	assert(th->dataFormat=="binary" || th->dataFormat=="text");
	sys->cur_thread_pool->addJob(th);
	return NULL;
}

void URLLoader::execute()
{
	CurlDownloader curlDownloader;

	bool done=curlDownloader.download(url);
	if(done)
	{
		if(dataFormat=="binary")
		{
			ByteArray* byteArray=Class<ByteArray>::getInstanceS(true);
			byteArray->acquireBuffer(curlDownloader.getBuffer(),curlDownloader.getLen());
			data=byteArray->obj;
		}
		else if(dataFormat=="text")
		{
			if(curlDownloader.getLen())
				abort();
			data=Class<ASString>::getInstanceS(true)->obj;
		}
		//Send a complete event for this object
		sys->currentVm->addEvent(this,Class<Event>::getInstanceS(true,"complete",obj));
	}
	else
	{
		//Notify an error during loading
		sys->currentVm->addEvent(this,Class<Event>::getInstanceS(true,"ioError",obj));
	}
}

ASFUNCTIONBODY(URLLoader,_getDataFormat)
{
	URLLoader* th=static_cast<URLLoader*>(obj->implementation);
	return Class<ASString>::getInstanceS(true,th->dataFormat)->obj;
}

ASFUNCTIONBODY(URLLoader,_getData)
{
	URLLoader* th=static_cast<URLLoader*>(obj->implementation);
	if(th->data==NULL)
		return new Undefined;
	
	th->data->incRef();
	return th->data;
}

ASFUNCTIONBODY(URLLoader,_setDataFormat)
{
	URLLoader* th=static_cast<URLLoader*>(obj->implementation);
	assert(args[0]);
	th->dataFormat=args[0]->toString();
	return NULL;
}

void URLLoaderDataFormat::sinit(Class_base* c)
{
	c->setVariableByQName("VARIABLES","",Class<ASString>::getInstanceS(true,"variables")->obj);
	c->setVariableByQName("TEXT","",Class<ASString>::getInstanceS(true,"text")->obj);
	c->setVariableByQName("BINARY","",Class<ASString>::getInstanceS(true,"binary")->obj);
}

void SharedObject::sinit(Class_base* c)
{
};

void ObjectEncoding::sinit(Class_base* c)
{
	c->setVariableByQName("AMF0","",new Integer(0));
	c->setVariableByQName("AMF3","",new Integer(3));
	c->setVariableByQName("DEFAULT","",new Integer(3));
};

NetConnection::NetConnection()
{
}

void NetConnection::sinit(Class_base* c)
{
	//assert(c->constructor==NULL);
	//c->constructor=new Function(_constructor);
	c->super=Class<EventDispatcher>::getClass();
	c->max_level=c->super->max_level+1;
}

void NetConnection::buildTraits(ASObject* o)
{
	o->setVariableByQName("connect","",new Function(connect));
}

void NetConnection::execute()
{
	abort();
}

ASFUNCTIONBODY(NetConnection,connect)
{
	NetConnection* th=Class<NetConnection>::cast(obj->implementation);
	assert(argslen==1);
	if(args[0]->getObjectType()!=T_UNDEFINED)
		abort();

	//When the URI is undefined the connect is successful (tested on Adobe player)
	Event* status=Class<NetStatusEvent>::getInstanceS(true);
	ASObject* info=new ASObject;
	info->setVariableByQName("level","",Class<ASString>::getInstanceS(true,"status")->obj);
	info->setVariableByQName("code","",Class<ASString>::getInstanceS(true,"NetConnection.Connect.Success")->obj);
	status->obj->setVariableByQName("info","",info);
	getVm()->addEvent(th, status);
	return NULL;
}

NetStream::NetStream()
{
}

void NetStream::sinit(Class_base* c)
{
	assert(c->constructor==NULL);
	c->constructor=new Function(_constructor);
	c->super=Class<EventDispatcher>::getClass();
	c->max_level=c->super->max_level+1;
}

void NetStream::buildTraits(ASObject* o)
{
	o->setVariableByQName("play","",new Function(play));
	o->setGetterByQName("bytesLoaded","",new Function(getBytesLoaded));
	o->setGetterByQName("bytesTotal","",new Function(getBytesTotal));
	o->setGetterByQName("time","",new Function(getTime));
}

ASFUNCTIONBODY(NetStream,_constructor)
{
	cout << "NetStream constructor"  << endl;
	return NULL;
}

ASFUNCTIONBODY(NetStream,play)
{
//	NetStream* th=Class<NetStream>::cast(obj->implementation);
	assert(argslen==1);
//	const tiny_string& arg0=args[0]->toString();
//	cout << arg0 << endl;
	return NULL;
}

ASFUNCTIONBODY(NetStream,getBytesLoaded)
{
	return abstract_i(1);
}

ASFUNCTIONBODY(NetStream,getBytesTotal)
{
	return abstract_i(100);
}

ASFUNCTIONBODY(NetStream,getTime)
{
	return abstract_d(0.1);
}

void URLVariables::sinit(Class_base* c)
{
	assert(c->constructor==NULL);
	c->constructor=new Function(_constructor);
}

ASFUNCTIONBODY(URLVariables,_constructor)
{
	assert(argslen==0);
	return NULL;
}

bool URLVariables::toString(tiny_string& ret)
{
	//Should urlencode
	__asm__("int $3");
	abort();
	int size=obj->numVariables();
	for(int i=0;i<size;i++)
	{
		const tiny_string& tmp=obj->getNameAt(i);
		if(tmp=="")
			abort();
		ret+=tmp;
		ret+="=";
		ret+=obj->getValueAt(i)->toString();
		if(i!=size-1)
			ret+="&";
	}
	return true;
}

bool CurlDownloader::download(const tiny_string& s)
{
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	bool ret=false;
	if(curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, s.raw_buf());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
		res = curl_easy_perform(curl);
		if(res==0)
			ret=true;
		curl_easy_cleanup(curl);
	}

	return ret;
}

size_t CurlDownloader::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	CurlDownloader* th=static_cast<CurlDownloader*>(userp);
	memcpy(th->buffer + th->offset,buffer,size*nmemb);
	th->offset+=(size*nmemb);
	return size*nmemb;
}

size_t CurlDownloader::write_header(void *buffer, size_t size, size_t nmemb, void *userp)
{
	CurlDownloader* th=static_cast<CurlDownloader*>(userp);
	char* headerLine=(char*)buffer;
	if(strncmp(headerLine,"Content-Length: ",16)==0)
	{
		//Now read the length and allocate the byteArray
		assert(th->buffer==NULL);
		th->len=atoi(headerLine+16);
		th->buffer=new uint8_t[th->len];
	}
	return size*nmemb;
}

