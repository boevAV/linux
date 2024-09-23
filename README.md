# About web-server

Реализован *Web-Server*, использующий многопроцессный способ обработки клиентов через *fork()*. 

Веб-сервер работает в виде демона и обеспечивает базовую поддержку протокола *HTTP*, на уровне метода *GET*.

Веб-сервер работает со статическими текстовыми и двоичными данными, такими как изображения. Предусмотрен контроль и журналирование ошибок (через *syslog*). 

Реализована одновременная работа сервера с большим числом клиентов.
