#ifndef _FILE_TRANSFERER_H
#define _FILE_TRANSFERER_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif //_WIN32_WINNT

#include <string>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

typedef boost::shared_ptr<std::ofstream> ofstream_ptr;

class file_transferer{
private:
	char* file_data;
	size_t parts, part_size, last_part_size;
	std::vector<size_t> file_part_sizes;
	const static boost::regex re_download, re_upload;

public:
	file_transferer();
	file_transferer(const file_transferer &ft);
	~file_transferer();

	//����� ������ ���������, ��� ��������� ��������� ������ ��������� ��� ���� local_name ��� ������ remote_name
	bool is_upload_request(const std::string &request, std::string &local_name, std::string &remote_name) const;
	//������ � ������� � ������������ �����, ������� ������������ ����� ��� ����������
	bool is_download_request(const std::string &request) const;

	//���� is_download_request(request), �� �������� ������� ���� 
	bool recieve_file(const std::string &request, boost::asio::ip::tcp::socket &socket_);
	//���������� ��������� (download_request), � ������ ��� ����
	bool send_file(const std::string &local_name, const std::string &remote_name, boost::asio::ip::tcp::socket &socket_);
	//����������� ���� remote_name � ���������� ���������� 
	bool request_file(const std::string &local_name, const std::string &remote_name, boost::asio::ip::tcp::socket &socket_);
};

#endif //_FILE_TRANSFERER_H
