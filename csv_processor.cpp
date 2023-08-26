#include <iostream>
#include <format>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>

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
            
            //Update min, max and sum so we dont have to loop through the data to calculate it. 
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
        ~Column() {
            m_name.clear();
            m_data.clear();
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
    private:
        vector<Column*> m_columns;
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
                Column* col;
                //Tokenize each line with , a separator
                while(getline(is, token, ',')) { //use comma as delim for cutting string
                    if (line_num == 1) {
                        //Create Column objs from the column names in the first line
                        col = new Column(token);
                        m_columns.push_back(col);
                    } else {
                        std::size_t pos;
                        double val = 0.0;
                        try {
                            val = stod(token, &pos);
                        } catch (exception e) {
                            cerr << "Exception occured when converting text: '" << token << "' to a number at cell with column# " << col_num +1  << " and line# " << line_num << endl;
                            //0.0 will be added for that cell
                            m_columns[col_num]->addData(val);
                            continue;
                        }
                        if (pos != token.length()) {
                            cerr << "Text: '" << token << "' could not be converted to number at cell with column# " << col_num+1 << " and line# " << line_num << endl;
                            //0.0
                        }
                         m_columns[col_num]->addData(val);
                    }
                    col_num++;
                }
            }
        }
        void printAllColumnsStats(int thread_cnt ) {
            int col_cnt = 0; 
            while (col_cnt < m_columns.size()) {
                if (thread_cnt == 0 ) {
                    m_columns[col_cnt]->computeStats();
                    col_cnt++;   
                }
            }
            cout << "[Count, Mean, Std, Min, Max]" << endl;
            for(int col = 0; col<m_columns.size(); col++) {
                m_columns[col]->printStats();
            }
        }   
};

int main(int argc, char* argv[]) {
    std::cout << "Calculating stats from the provided csv ..." << endl;
    
    int thread_cnt = 0;
    if (argc < 2 ) {
        cerr << "csv filename is a required argument" << endl;
        cout << "Usage: csv_processor csv_file_name [number_of_threads]" << endl;
        exit(1);
    } else {
        size_t pos;
        string cf = argv[1];
        filesystem::path p{cf};
        Csv csv = Csv(p);
        if (argc > 2) {
            string thstr = argv[2];
            int th = stoi(thstr, &pos);
            if (pos == thstr.length()) {
                thread_cnt = th;
            }
        }
        csv.printAllColumnsStats(thread_cnt);
    }
}