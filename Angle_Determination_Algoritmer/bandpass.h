class Bandpass
{
    public:
        Bandpass();

        void filter();

    private:
        void lowpass_filter();
        void highpass_filter();
};