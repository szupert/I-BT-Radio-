from pathlib import Path
import hashlib
ORIGINAL='85755cb020d9671a4bf2e254c2be33e5506c93db65a921f7554f59ab0a4afc78'
HELPER='// I-BT-Radio: owner-only cancellation; no foreign-task close.\nextern "C" bool ibt_stream_open_cancelled() __attribute__((weak));\nstatic bool ibt_open_cancelled(){return ibt_stream_open_cancelled&&ibt_stream_open_cancelled();}\nstatic int ibt_connect_wait(int fd,int timeout){\n const uint32_t start=millis();\n while(uint32_t(millis()-start)<uint32_t(timeout)){\n  if(ibt_open_cancelled()){errno=ECANCELED;return -1;}\n  const unsigned elapsed=uint32_t(millis()-start);\n  if(elapsed>=unsigned(timeout))break;\n  const unsigned left=unsigned(timeout)-elapsed;\n  fd_set write;FD_ZERO(&write);FD_SET(fd,&write);\n  timeval slice={0,long(std::min(left,50u)*1000)};\n  const int n=select(fd+1,nullptr,&write,nullptr,&slice);\n  if(n!=0)return n;\n }return 0;\n}\n'
def transform(s):
 anchor='const char *pers = "esp32-tls";'
 assert s.count(anchor)==1;s=s.replace(anchor,HELPER+anchor)
 old='res = select(ssl_client->socket + 1, nullptr, &fdset, nullptr, timeout < 0 ? nullptr : &tv);'
 assert s.count(old)==1;s=s.replace(old,'res = ibt_connect_wait(ssl_client->socket, timeout);')
 old='while ((ret = mbedtls_ssl_handshake(&ssl_client->ssl_ctx)) != 0) {'
 assert s.count(old)==1;s=s.replace(old,'while (!ibt_open_cancelled() && (ret = mbedtls_ssl_handshake(&ssl_client->ssl_ctx)) != 0) {')
 old='  if (ssl_client->client_cert.version) {'
 assert s.count(old)==2;s=s.replace(old,'  if(ibt_open_cancelled())return MBEDTLS_ERR_SSL_INTERNAL_ERROR;\n'+old,1)
 return s
def apply(path,control):
 raw=control.read_bytes();assert hashlib.sha256(raw).hexdigest()==ORIGINAL,'Unexpected Arduino TLS baseline'
 expected=transform(raw.decode()).encode();actual=path.read_bytes()
 assert actual in (raw,expected),'Unknown Arduino TLS patch; refuse silent overwrite'
 if actual!=expected:path.write_bytes(expected)
 return hashlib.sha256(expected).hexdigest()
