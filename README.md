# install boost
1. download https://www.boost.org/users/history/
2. run bootstrap.bat
3. run b2.exe
4. run b2.exe 
5. run cmd -> b2.exe --toolset=msvc variant=release link=static threading=multi runtime-link=static stage
6. VS Проект/Свойства/С/С++/Дополнительные каталоги включаемых файлов -> ..\boost_1_75_0
7. VS Проект/Свойства/Компоновщик/Общие -> ..\boost_1_75_0\stage\lib

# Boost::RegEx:

// Общий макет, типа - ДвоичныеДанные
УстановитьВнешнююКомпоненту("ОбщийМакет.RegEx"); // только для клиента
Если ПодключитьВнешнююКомпоненту("ОбщийМакет.RegEx", "boost") Тогда
	
	//ru
	Regex = Новый("AddIn.boost.RegEx");
	Regex.Шаблон = "а.";
	Regex.Текст = "01234аё7ас";
	Пока Regex.Найти() Цикл 
		Сообщить(Regex.Позиция);
		Сообщить(Regex.Длина);
		Сообщить(Regex.Значение);
	КонецЦикла;
	
	//en
	Regex = Новый("AddIn.boost.RegEx");
	Regex.Pattern = "а.";
	Regex.Text = "01234аё7ас";
	Пока Regex.Найти() Цикл 
		Сообщить(Regex.Position);
		Сообщить(Regex.Length);
		Сообщить(Regex.Value);
	КонецЦикла;

КонецЕсли;