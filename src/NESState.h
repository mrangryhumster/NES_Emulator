#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>

class NESState
{
public:

	NESState()
	{
		m_Bytes.resize(1024);
		this->Clear();
	}

	void Read(void* dst, size_t size)
	{
		if ((m_Position + size) > m_Bytes.size())
		{
			printf("NESState ERROR: Read operation exceed data size \n");
			return;
		}
		memcpy(dst, &(m_Bytes[m_Position]), size);
		m_Position += size;
	}
	
	void Write(void* src, size_t size)
	{
		//'Write completed' marker
		if (src == nullptr)
		{
			m_IsValid = true;
			return;
		}

		if (m_Bytes.size() < (m_Position + size))
		{
			m_Bytes.resize(m_Position + size);
		}
		memcpy(&(m_Bytes[m_Position]), src, size);
		m_Position += size;
	}

	void Seek(size_t position)
	{
		m_Position = position;
	}

	bool IsValid()
	{
		return m_IsValid;
	}

	void Clear()
	{
		m_Position = 0;
		m_IsValid = false;
	}

	static bool LoadFromFile(std::string file_name, NESState* states, size_t count)
	{
		if (!std::filesystem::exists(file_name))
		{
			printf("Unable to load savestates : file not found (it's probably fine)\n");
			return true;
		}

		std::ifstream ifs(file_name, std::ifstream::binary);
		if (ifs.is_open())
		{
			//Read and check header
			uint64_t magic;
			ifs.read((char*)&magic, sizeof(uint64_t));

			if (ifs.fail())
			{
				printf("Unable to load savestates : stream fail (at read:header)\n");
				return false;
			}

			if (magic != NESState::FileMagic)
			{
				printf("Unable to load savestates : incompatible version\n");
				return false;
			}

			//Load states one by one
			for (size_t stateId = 0; stateId < count; stateId++)
			{
				//read marker
				uint32_t marker;
				ifs.read((char*)&marker, sizeof(uint32_t));

				//check fail
				if (ifs.fail())
				{
					printf("Unable to load savestates : stream fail (at read:marker)\n");
					return false;
				}

				//skip state if empty marker read
				if (marker == EmptyMarker)
					continue;

				uint32_t data_size = 0;
				ifs.read((char*)&data_size, sizeof(uint32_t));
				//check fail
				if (ifs.fail())
				{
					printf("Unable to load savestates : stream fail (at read:size)\n");
					return false;
				}

				//read state
				ifs.read((char*)states[stateId].m_Bytes.data(), data_size);

				//check fail
				if (ifs.fail())
				{
					printf("Unable to load savestates : stream fail (at read:header)\n");
					return false;
				}

				printf("Save state slot #%d loaded\n", (uint32_t)stateId);
				states[stateId].m_IsValid = true;

				if (ifs.eof())return true;
			}
			//Close stream
			ifs.close();
		}
		else
		{
			return false;
		}
		return true;
	}

	static bool SaveToFile(std::string file_name, NESState* states, size_t count)
	{
		const uint64_t FileMagic = NESState::FileMagic;
		const uint32_t StateMarker = NESState::StateMarker;
		const uint32_t EmptyMarker = NESState::EmptyMarker;
		std::ofstream ofs(file_name, std::ofstream::binary);
		if (ofs.is_open())
		{
			//Write header
			ofs.write((const char*)&FileMagic, sizeof(uint64_t));
			if (ofs.fail()) 
			{ 
				printf("Unable to save savestates : stream fail (at write:header)\n");
				return false;
			}

			//Write states one by one
			for (size_t stateId = 0; stateId < count; stateId++)
			{
				//write markers
				if (states[stateId].IsValid())
				{
					ofs.write((const char*)&StateMarker, sizeof(uint32_t));
				}
				else
				{
					ofs.write((const char*)&EmptyMarker, sizeof(uint32_t));

					//check fail
					if (ofs.fail())
					{
						printf("Unable to save savestates : stream fail (at write:marker)\n");
						return false;
					}
					continue;
				}

				uint32_t data_size = (uint32_t)states[stateId].m_Bytes.size();
				ofs.write((char*)&data_size, sizeof(uint32_t));
				//check fail
				if (ofs.fail())
				{
					printf("Unable to save savestates : stream fail (at write:size)\n");
					return false;
				}

				//write state
				ofs.write((char*)states[stateId].m_Bytes.data(), states[stateId].m_Bytes.size());

				//check fail
				if (ofs.fail())
				{
					printf("Unable to save savestates : stream fail (at write:data)\n");
					return false;
				}
			}
			//Close stream
			ofs.close();
		}
		else
		{
			return false;
		}
		return true;
	}

private:
	std::vector<uint8_t> m_Bytes;
	size_t				 m_Position;
	bool				 m_IsValid;

	static const uint64_t FileMagic = 0x1002000073656E65;
	static const uint32_t StateMarker = 0x00C0FE00;
	static const uint32_t EmptyMarker = 0x00DEAD00;
};