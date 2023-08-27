#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
#include <thread>
#include <memory>

using namespace std;

class Column {
    //Hold data for one column
    const int PRECISION = 20; 
    private:
        string m_name;
        vector<double> m_data;
        double m_min;
        double m_max;
        double m_sum;
        double m_std;
        double m_mean;
    public:
        Column(string_view name)
        {
            m_name = name;
        }
        void addData(double val) {
            
            //Update min, max and sum so we dont have to loop through the data later to calculate it. 
            //We stil have to loop once to calculate the std but not twice
            if (m_data.size() == 0 ) {
                m_max = val;
                m_min = val;
                m_sum = val;
            } else {
                if (val < m_min) {
                    m_min = val;
                }
                if (val> m_max) {
                    m_max = val;
                }
                m_sum += val;
            }
            m_data.push_back(val);
        }
        
        void computeStats(void) {
            m_mean = m_sum / m_data.size();
            double var = 0;
            for (int i = 0; i < m_data.size(); i++) {
                double diff = m_data[i]-m_mean; 
                var += pow(diff, 2);
            }
            m_std = sqrt(var);
        }
        void printStats() {
            cout << setprecision(PRECISION);
            cout << '[' << m_data.size() << ", " << m_mean << ", " << m_std << ", " << m_min << ", " << m_max << ']' << endl; 
        }
};

class Csv {
    //class that parses the csv file, extracts and stores data and prints the stats
    private:
        vector<Column> m_columns;
        filesystem::path m_filename {};

    public:
        Csv(filesystem::path f) {
            m_filename = f;
            ifstream* csvfile;
            try
            {
                csvfile = new ifstream(f, ios_base::beg);
            }
            catch(const std::exception& e)
            {
                cerr << "Error while opening file: " << m_filename << endl;
                std::cerr << e.what() << '\n';
                exit(1);
            }

            int line_num = 0;
            while (!(*csvfile).eof()) {
                line_num++;
                string line, token;
                *csvfile >> line;
                //Remove \n if present
                size_t last_idx = line.length()-1;
                if (line[last_idx] == '\n') line.erase(last_idx, 1);
                istringstream is {line};
                vector<string> tokens;
                int col_num {0};
                //Parse the csv to populate the Column objects
                //Tokenize each line with a ','  separator
                while(getline(is, token, ',')) { 
                    if (line_num == 1) {
                        //Create Column objs from the column names in the first line
                        Column col = Column(token);
                        m_columns.push_back(col);
                    } else {
                        size_t pos;
                        double val = 0.0;
                        try {
                            val = stod(token, &pos);
                        } catch (exception e) {
                            cerr << "Exception occured when converting text: '" << token << "' to a number in cell with column# " << col_num +1  << " and line# " << line_num << endl;
                            //Default of 0.0 will be added for this cell
                            m_columns[col_num].addData(val);
                            continue;
                        }
                        if (pos != token.length()) {
                            cerr << "Text: '" << token << "' could not be converted to number in cell with column# " << col_num+1 << " and line# " << line_num << endl;
                            //Default of 0.0 will be added for this cell
                        }
                         m_columns[col_num].addData(val);
                    }
                    col_num++;
                }
            }
            delete csvfile;
        }

        void launchThreadBatch(int col_n, int cols) {
            //Call computeStats for cols number of columns starting at column# col_n. Called directly each worked thread
            for(int n = 0; n < cols; n++) {
                m_columns[col_n++].computeStats();
            }                                    
        }  

        void printAllColumnsStats(int thread_cnt ) {
            //Main function to compute all threads with multi threading and then print them
            int cols_per_batch = m_columns.size() / thread_cnt;
            int rem = m_columns.size() % thread_cnt;
            int num_batches = (m_columns.size() - rem) / cols_per_batch;
            int col_cnt = 0; 
            vector<unique_ptr<thread>> threads;
            //Compute stats for each column using the number of worker thread that the user requested or in a single threaded mode 
            while (col_cnt < m_columns.size()) {
                if (thread_cnt == 1 ) {
                    //Processing in main application thread
                    m_columns[col_cnt].computeStats();
                    col_cnt++;   
                } else {
                    int b_n = 0;
                    //Create one batch less the number calculated as the last batch will run on the main thread
                    while (b_n < num_batches-1) {
                        cout << "Launching thread# " << b_n+1 << endl;
                        unique_ptr<thread> t { new thread {&Csv::launchThreadBatch, this, col_cnt, cols_per_batch}};
                        threads.push_back(move(t));
                        col_cnt += cols_per_batch;                 
                        b_n++;
                    }
                    //Reset thread_cnt to 1 so the remaining columns are processed in the main app thread after 
                    //the requested number of workers have been launched
                    thread_cnt = 1;
                }                
            }
            //Join all threads before calling the print functions
            for (int i = 0; i<threads.size(); i++) {
                threads[i]->join();
            }
            cout << "[Count, Mean, Std, Min, Max]" << endl;
            //Call print for each column
            for(int col = 0; col<m_columns.size(); col++) {
                m_columns[col].printStats();
            }
        }   
};

int main(int argc, char* argv[]) {
    std::cout << "Calculating stats from the provided csv ..." << endl;
    
    int thread_cnt = 1;
    if (argc < 2 ) {
        cerr << "csv filename is a required argument" << endl;
        cout << "Usage: csv_processor csv_file_name [number_of_worker_threads]" << endl;
        exit(1);
    } else {
        size_t pos;
        string cf = argv[1];
        filesystem::path p{cf};
        Csv csv = Csv(p);
        if (argc > 2) {
            //User has requested multi threading. Anything after second argument will be silently ignored
            string thstr = argv[2];
            int th = 0;
            try
            {
                 th = stoi(thstr, &pos);
            }
            catch(const std::exception& e)
            {
                cerr <<"Non numeric value for the number of threads. " << e.what() << '\n';
            }
            if (pos == thstr.length()) {
                thread_cnt += th;
            }
        }
        csv.printAllColumnsStats(thread_cnt);
    }
}