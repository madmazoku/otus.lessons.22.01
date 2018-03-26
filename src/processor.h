#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <ctime>
#include <tuple>
#include <map>

#include <boost/lexical_cast.hpp>

#include "queue_processor.h"
#include "metrics.h"

using Command = std::tuple<time_t, std::string>;
using Commands = std::vector<Command>;

std::ostream& operator<<(std::ostream& out, const Command& command)
{
    out << " {" << std::get<0>(command) << ", " << std::get<1>(command) << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const Commands& commands)
{
    for(auto c : commands)
        std::cerr << ' ' << c;
    return out;
}

class Processor : public QueueProcessor<Commands>
{
public:

    Processor(size_t thread_count) : QueueProcessor<Commands>()
    {
        start(thread_count);
    }
    virtual ~Processor() = default;

    void process(const Commands& commands)
    {
        add(commands, true);
    }
};
using Processors = std::vector<Processor*>;

class ProcessorSubscriber : public QueueProcessor<std::string>
{
private:
    Processors _processors;
    std::mutex _processors_mutex;

public:
    ProcessorSubscriber(size_t thread_count) : QueueProcessor<std::string>()
    {
        start(thread_count);
    }
    virtual ~ProcessorSubscriber() = default;

    void process(const Commands& commands)
    {
        for(auto p : _processors)
            p->process(commands);
    }

    void subscribe(Processor* processor)
    {
        std::lock_guard<std::mutex> lock_processors(_processors_mutex);
        _processors.push_back(processor);
    }
};

class ConsolePrint : public Processor
{
private:
    Metrics* _metrics;

    virtual void act(const Commands& commands, size_t qid) final {
        if(commands.size() == 0)
            return;

        _metrics->update("console.blocks", 1);
        _metrics->update("console.commands", commands.size());

        std::cout << "bulk: ";
        bool first = true;
        for(auto c :commands)
        {
            if(first)
                first = false;
            else
                std::cout << ", ";
            std::cout << std::get<1>(c);
        }
        std::cout << std::endl;
    }

public:
    ConsolePrint(Metrics* metrics) : Processor(1), _metrics(metrics) { }
    virtual ~ConsolePrint() = default;
};

class FilePrint : public Processor
{
private:
    Metrics* _metrics;
    std::map<time_t, size_t> _log_counter;
    std::mutex _log_counter_mutex;

    virtual void act(const Commands& commands, size_t qid) final {
        if(commands.size() == 0)
            return;

        std::string prefix = "file.";
        _metrics->update(prefix + "blocks");
        _metrics->update(prefix + "commands", commands.size());
        prefix += boost::lexical_cast<std::string>(qid);
        _metrics->update(prefix + ".blocks");
        _metrics->update(prefix + ".commands", commands.size());

        std::string name = "bulk";
        time_t tm = std::get<0>(*(commands.begin()));
        name += boost::lexical_cast<std::string>(tm);
        name += "-";

        size_t log_counter = 0;
        {
            std::lock_guard<std::mutex> lock_log_counter(_log_counter_mutex);
            auto it_cnt = _log_counter.find(tm);
            if(it_cnt != _log_counter.end())
                log_counter = ++(it_cnt->second);
            else
                _log_counter[tm] = log_counter;
        }
        std::string cnt = boost::lexical_cast<std::string>(log_counter);

        std::ofstream out(name + cnt + ".log");
        for(auto c :commands)
            out << std::get<1>(c) << std::endl;
        out.close();
    }

public:
    FilePrint(Metrics* metrics) : Processor(2), _metrics(metrics) { }
    virtual ~FilePrint() = default;
};

class Reader : public ProcessorSubscriber
{
private:
    Metrics* _metrics;
    size_t _N;

    std::mutex _commands_mutex;

    std::string _buffer;
    Commands _commands;
    size_t _bracket_counter;

    void flush()
    {
        if(_commands.size() > 0) {
            _metrics->update("reader.blocks");
            _metrics->update("reader.commands", _commands.size());

            process(_commands);
            _commands.clear();
        }
    }

    void process_line(const std::string& line)
    {
        _metrics->update("reader.line_count");
        _metrics->update("reader.line_size", line.size());

        if(line == "{") {
            if(_bracket_counter++ == 0)
                flush();
        } else if(line == "}") {
            if(_bracket_counter > 0 && --_bracket_counter == 0)
                flush();
        } else {
            _commands.push_back(std::make_tuple(std::time(nullptr), line));
            if(_bracket_counter == 0 && _N != 0 && _commands.size() == _N)
                flush();
        }
    }

    virtual void act(const std::string& data, size_t qid) final {
        _metrics->update("reader.push_count");
        _metrics->update("reader.push_size", data.size());

        std::lock_guard<std::mutex> lock_thread(_commands_mutex);

        _buffer.append(data);
        size_t start_pos = 0;
        while(true)
        {
            size_t end_pos = _buffer.find('\n', start_pos);
            if(end_pos != std::string::npos) {
                process_line(_buffer.substr(start_pos, end_pos - start_pos));
                start_pos = end_pos;
                ++start_pos;
            } else {
                _buffer.erase(0, start_pos);
                break;
            }
        }
    }

public:
    Reader(Metrics* metrics, size_t N = 0) : ProcessorSubscriber(1), _metrics(metrics), _N(N), _bracket_counter(0)  {}
    virtual ~Reader() = default;

    void read(std::istream& in)
    {
        char buffer[16];
        while(!std::cin.eof()) {
            std::cin.read(buffer, 16);
            push(buffer, std::cin.gcount());
        }
        done();
    }

    void push(const char* data, size_t data_size)
    {
        add(std::string{data, data_size}, true);
    }

    virtual void done()
    {
        wait();

        {
            std::lock_guard<std::mutex> lock_thread(_commands_mutex);
            if(_bracket_counter == 0)
                flush();
        }

        ProcessorSubscriber::done();
    }
};
