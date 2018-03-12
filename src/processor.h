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

std::ostream& operator<<(std::ostream& out, const Command& command) {
    out << " {" << std::get<0>(command) << ", " << std::get<1>(command) << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const Commands& commands) {
    for(auto c : commands)
        std::cerr << ' ' << c;
    return out;
}

class Processor : public QueueProcessor<Commands>
{
public:

    Processor(size_t thread_count) : QueueProcessor<Commands>() {
        start(thread_count);
    }
    virtual ~Processor() {
        std::cout << "~Processor" << std::endl;
        done();
        std::cout << "end ~Processor" << std::endl;
    }

    void process(const Commands& commands) {
        add(commands, true);
    }

    void done() {
        std::cout << "done" << std::endl;
        stop();
        wait();
        finish();
        std::cout << "end done" << std::endl;
    }
};
using Processors = std::vector<Processor*>;

class ProcessorSubscriber
{
private:
    Processors _processors;
    std::mutex _processors_mutex;

public:

    virtual ~ProcessorSubscriber() {
        std::cout << "~ProcessorSubscriber" << std::endl;
    }

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
    virtual void act(const Commands& commands, size_t qid) final {
        if(commands.size() == 0)
            return;

        Metrics::get_metrics().update("console.blocks", 1);
        Metrics::get_metrics().update("console.commands", commands.size());

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
    ConsolePrint() : Processor(1) { 
        std::cout << "ConsolePrint" << std::endl;
    }
    virtual ~ConsolePrint() { 
        std::cout << "~ConsolePrint" << std::endl;
    }

    static ConsolePrint* get_singleton() {
        static ConsolePrint singleton;
        return &singleton;
    }
};

class FilePrint : public Processor
{
private:
    std::map<time_t, size_t> _log_counter;
    std::mutex _log_counter_mutex;

    virtual void act(const Commands& commands, size_t qid) final {
        if(commands.size() == 0)
            return;

        std::string prefix = "file.";
        Metrics::get_metrics().update(prefix + "blocks");
        Metrics::get_metrics().update(prefix + "commands", commands.size());
        prefix += boost::lexical_cast<std::string>(qid);
        Metrics::get_metrics().update(prefix + ".blocks");
        Metrics::get_metrics().update(prefix + ".commands", commands.size());

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
    FilePrint() : Processor(2) { }
    virtual ~FilePrint() { 
        std::cout << "~FilePrint" << std::endl;
    }

    static FilePrint* get_singleton() {
        static FilePrint singleton;
        return &singleton;
    }
};

class Reader : public ProcessorSubscriber
{
private:
    size_t _N;

    std::string _buffer;
    Commands _commands;
    size_t _bracket_counter;

    void flush()
    {
        if(_commands.size() > 0) {
            Metrics::get_metrics().update("reader.blocks");
            Metrics::get_metrics().update("reader.commands", _commands.size());
std::cout << _commands << std::endl;

            process(_commands);
            _commands.clear();
        }
    }

    void process_line(const std::string& line) {
        Metrics::get_metrics().update("reader.line_count");
        Metrics::get_metrics().update("reader.line_size", line.size());
        std::cout << "line: '" << line << "'" << std::endl;
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

    virtual void act(const std::string& data) {
        Metrics::get_metrics().update("reader.push_count");
        Metrics::get_metrics().update("reader.push_size", data.size());

        _buffer.append(data);
        size_t start_pos = 0;
        while(true) {
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
    Reader(size_t N = 0) : _N(N), _bracket_counter(0)  {
        std::cout << "Reader" << std::endl;
    }
    virtual ~Reader() { 
        std::cout << "~Reader" << std::endl;
    }

    void read(std::istream& in)
    {
        char buffer[16];
        while(!std::cin.eof()) {
            std::cin.read(buffer, 16);
            push(buffer, std::cin.gcount());
        }
        done();
    }

    void push(const char* data, size_t data_size) {
        act(std::string{data, data_size});
    }

    void done() {
        if(_bracket_counter == 0)
            flush();
    }
};
