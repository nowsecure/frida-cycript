struct CYExpression {
};

struct CYToken {
    virtual const char *Text() const = 0;
};

struct CYTokenPrefix :
    virtual CYToken
{
};

struct CYTokenInfix :
    virtual CYToken
{
    virtual unsigned Precedence() const = 0;
};

struct CYTokenPostfix :
    virtual CYToken
{
};

struct CYTokenAssignment :
    virtual CYToken
{
};

struct CYTokenAccess :
    virtual CYToken
{
};

struct CYTokenLiteral :
    CYExpression,
    virtual CYToken
{
};

struct CYTokenString :
    CYTokenLiteral
{
};

struct CYTokenNumber :
    CYTokenLiteral
{
};

struct CYTokenWord :
    virtual CYToken
{
};

struct CYTokenIdentifier :
    CYExpression,
    CYTokenWord
{
    const char *word_;

    virtual const char *Text() const {
        return word_;
    }
};

struct CYTokenColon :
    CYToken
{
};

struct CYTokenComma :
    CYToken
{
};

struct CYTokenSemi :
    CYToken
{
};

struct CYTokenQuestion :
    CYToken
{
};

std::set<const char *, CStringMapLess> OperatorWords_;

struct CYParser {
    FILE *fin_;
    FILE *fout_;

    size_t capacity_;
    char *data_;

    size_t offset_;
    size_t size_;

    CYPool pool_;

    CYParser(FILE *fin, FILE *fout) :
        fin_(fin),
        fout_(fout),
        capacity_(1024),
        data_(reinterpret_cast<char *>(malloc(capacity_))),
        offset_(0),
        size_(0)
    {
    }

    ~CYParser() {
        // XXX: this will not deconstruct in constructor failures
        free(data_);
    }

    bool ReadLine(const char *prompt) {
        offset_ = 0;
        data_[capacity_ - 1] = ~'\0';

      start:
        if (fout_ != NULL) {
            fputs(prompt, fout_);
            fputs(" ", fout_);
            fflush(fout_);
        }

        if (fgets(data_, capacity_, fin_) == NULL)
            return false;

      check:
        if (data_[capacity_ - 1] != '\0') {
            size_ = strlen(data_);
            if (size_ == 0)
                goto start;
            if (data_[size_ - 1] == '\n') {
                --size_;
                goto newline;
            }
        } else if (data_[capacity_ - 2] == '\n') {
            size_ = capacity_ - 2;
          newline:
            data_[size_] = '\0';
        } else {
            size_t capacity(capacity_ * 2);
            char *data(reinterpret_cast<char *>(realloc(data_, capacity)));
            _assert(data != NULL);
            data_ = data;
            size_ = capacity_ - 1;
            capacity_ = capacity;
            fgets(data_ + size_, capacity_ - size_, fin_);
            goto check;
        }

        return true;
    }

    _finline void ScanRange(const CYRange &range) {
        while (range[data_[offset_]])
            ++offset_;
    }

    CYToken *ParseToken(const char *prompt) {
        char next;

        do {
            if (offset_ == size_ && (prompt == NULL || !ReadLine(prompt)))
                return false;
            next = data_[offset_++];
        } while (next == ' ' || next == '\t');

        CYTokenType type;
        size_t index(offset_ - 1);

        if (WordStartRange_[next]) {
            ScanRange(WordEndRange_);
            type = CYTokenWord;
        } else if (next == '.') {
            char after(data_[offset_]);
            if (after >= '0' && after <= '9')
                goto number;
            goto punctuation;
        } else if (next >= '0' && next <= '9') {
          number:
            ScanRange(NumberRange_);
            type = CYTokenLiteral;
        } else if (PunctuationRange_[next]) {
          punctuation:
            ScanRange(PunctuationRange_);
            type = CYTokenPunctuation;
        } else if (next == '"' || next == '\'') {
            for (;;) {
                char after(data_[offset_++]);
                if (after == '\\') {
                    after = data_[offset_++];
                    _assert(after != '\0');
                    if (after == 'u') {
                        offset_ += 4;
                        _assert(offset_ < size_);
                    }
                } else if (after == next)
                    break;
            }

            type = CYTokenLiteral;
        } else if (next == '(' || next == '{' || next == '[') {
            type = CYTokenOpen;
        } else if (next == ')' || next == '}' || next == ']') {
            type = CYTokenClose;
        } else if (next == ';') {
            type = CYTokenSemiColon;
        } else {
            printf(":( %u\n", next);
            _assert(false);
        }

        char *value(pool_(data_ + index, offset_ - index));

        if (type == CYTokenWord && OperatorWords_.find(value) != OperatorWords_.end())
            type = CYTokenPunctuation;

        CYToken *token(new(pool_) CYToken());
        token->type_ = type;
        token->value_ = value;
        token->next_ = token;
        token->prev_ = &token->next_;
        return token;
    }

    CYToken *ParseExpression(const char *prompt) {
        CYToken *token(ParseToken(prompt));
        return token;
    }
};
