#include <corokafka/corokafka.h>
#include <gtest/gtest.h>
#include <corokafka_tests_utils.h>

namespace Bloomberg {
namespace corokafka {
namespace tests {

const int MaxLoops = 60; //arbitrary wait value
const int NumPartitions = 4;

void waitUntilEof()
{
    int loops = MaxLoops;
    while (callbackCounters()._eof == 0 && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

std::string getNewGroupName()
{
    static size_t i = std::chrono::system_clock::now().time_since_epoch().count();
    return "group_" + std::to_string(i++);
}

//topic config
Configuration::OptionList consumerTopicConfig = {
    {TopicConfiguration::Options::brokerTimeoutMs, 5000}
};

//paused on start with auto commit every 10ms
Configuration::OptionList config1 = {
    {"enable.partition.eof", true},
    {"enable.auto.offset.store", false},
    {"enable.auto.commit", false},
    {"auto.offset.reset","beginning"},
    {"auto.commit.interval.ms", 10},
    //{"debug","all"},
    {"topic.metadata.refresh.interval.ms", 5000},
    {ConsumerConfiguration::Options::timeoutMs, 100},
    {ConsumerConfiguration::Options::pauseOnStart, true},
    {ConsumerConfiguration::Options::readSize, 100},
    {ConsumerConfiguration::Options::pollStrategy, "batch"},
    {ConsumerConfiguration::Options::offsetPersistStrategy, "store"},
    {ConsumerConfiguration::Options::commitExec, "sync"},
    {ConsumerConfiguration::Options::autoOffsetPersist, "true"},
    {ConsumerConfiguration::Options::receiveInvokeThread, "coro"},
    {ConsumerConfiguration::Options::preprocessMessages, "false"},
    {ConsumerConfiguration::Options::receiveCallbackThreadRangeLow, 1},
    {ConsumerConfiguration::Options::receiveCallbackThreadRangeHigh, 1},
    {ConsumerConfiguration::Options::preserveMessageOrder, true},
};

//same as config1 but with auto-commit turned off, so the application can do it manually
//not paused on start.
Configuration::OptionList config2 = {
    {"enable.partition.eof", true},
    {"enable.auto.offset.store", false},
    {"enable.auto.commit", false},
    {"auto.offset.reset","beginning"},
    {"auto.commit.interval.ms", 10},
    //{"debug","all"},
    {"topic.metadata.refresh.interval.ms", 5000},
    {ConsumerConfiguration::Options::pauseOnStart, false},
    {ConsumerConfiguration::Options::readSize, 100},
    {ConsumerConfiguration::Options::pollStrategy, "roundrobin"},
    {ConsumerConfiguration::Options::offsetPersistStrategy, "commit"},
    {ConsumerConfiguration::Options::commitExec, "async"},
    {ConsumerConfiguration::Options::autoOffsetPersist, "false"},
    {ConsumerConfiguration::Options::receiveInvokeThread, "coro"},
    {ConsumerConfiguration::Options::preprocessMessages, "true"},
    {ConsumerConfiguration::Options::preserveMessageOrder, true},
};

//same as group 2 but pre-processing done on coroutine thread & round-robin
Configuration::OptionList config3 = {
    {"enable.partition.eof", true},
    {"enable.auto.offset.store", false},
    {"enable.auto.commit", false},
    {"auto.offset.reset","beginning"},
    {"auto.commit.interval.ms", 10},
    //{"debug","all"},
    {"topic.metadata.refresh.interval.ms", 5000},
    {ConsumerConfiguration::Options::pauseOnStart, false},
    {ConsumerConfiguration::Options::readSize, 100},
    {ConsumerConfiguration::Options::pollStrategy, "roundrobin"},
    {ConsumerConfiguration::Options::offsetPersistStrategy, "commit"},
    {ConsumerConfiguration::Options::commitExec, "async"},
    {ConsumerConfiguration::Options::autoOffsetPersist, "false"},
    {ConsumerConfiguration::Options::receiveInvokeThread, "io"},
    {ConsumerConfiguration::Options::preprocessMessages, "true"},
    {ConsumerConfiguration::Options::preserveMessageOrder, true},
};

//same as config 2 but poll strategy is serial
Configuration::OptionList config4 = {
    {"enable.partition.eof", true},
    {"enable.auto.offset.store", false},
    {"enable.auto.commit", false},
    {"auto.offset.reset","beginning"},
    {"auto.commit.interval.ms", 10},
    //{"debug","all"},
    {"topic.metadata.refresh.interval.ms", 5000},
    {ConsumerConfiguration::Options::pauseOnStart, true},
    {ConsumerConfiguration::Options::readSize, 100},
    {ConsumerConfiguration::Options::pollStrategy, "serial"},
    {ConsumerConfiguration::Options::offsetPersistStrategy, "commit"},
    {ConsumerConfiguration::Options::commitExec, "async"},
    {ConsumerConfiguration::Options::autoOffsetPersist, "false"},
    {ConsumerConfiguration::Options::receiveInvokeThread, "coro"},
    {ConsumerConfiguration::Options::preprocessMessages, "true"},
    {ConsumerConfiguration::Options::preserveMessageOrder, true},
};

TEST(ConsumerConfiguration, MissingBrokerList)
{
    ConsumerConfiguration config(
            topicWithHeaders(), {}, {}, Callbacks::messageReceiverWithHeaders); //use all defaults
    ConfigurationBuilder builder;
    builder(config);
    ASSERT_THROW(Connector connector(builder, dispatcher()), InvalidOptionException);
    try { Connector connector(builder, dispatcher()); }
    catch(const InvalidOptionException& ex) {
        ASSERT_STREQ("metadata.broker.list", ex.option());
    }
}

TEST(ConsumerConfiguration, MissingGroupId)
{
    ConsumerConfiguration config(
            topicWithHeaders(), {{"metadata.broker.list", programOptions()._broker}}, {}, Callbacks::messageReceiverWithHeaders); //use all defaults
    ConfigurationBuilder builder;
    builder(config);
    ASSERT_THROW(Connector connector(builder, dispatcher()), InvalidOptionException);
    try { Connector connector(builder, dispatcher()); }
    catch(const InvalidOptionException& ex) {
        ASSERT_STREQ("group.id", ex.option());
    }
}

TEST(ConsumerConfiguration, UnknownOption)
{
    ConsumerConfiguration config(topicWithHeaders(),
        {{"metadata.broker.list", programOptions()._broker},
         {"group.id", "test-group"},
         {"somebadoption", "bad"}}, {},
         Callbacks::messageReceiverWithHeaders);
    ConfigurationBuilder builder;
    builder(config);
    ASSERT_THROW(Connector connector(builder, dispatcher()), InvalidOptionException);
}

TEST(ConsumerConfiguration, UnknownInternalOption)
{
    ASSERT_THROW(ConsumerConfiguration config(topicWithHeaders(),
        {{"metadata.broker.list", programOptions()._broker},
         {"internal.consumer.unknown.option", "bad"}}, {},
         Callbacks::messageReceiverWithHeaders), InvalidOptionException);
}

TEST(ConsumerConfiguration, InternalConsumerPauseOnStart)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.pause.on.start",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerTimeoutMs)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.timeout.ms",
        {{"-2",true},{"1000",false}});
}

TEST(ConsumerConfiguration, InternalConsumerPollTimeoutMs)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.poll.timeout.ms",
        {{"-2",true},{"1000",false}});
}

TEST(ConsumerConfiguration, InternalConsumerRoundRobinMinPollTimeoutMs)
{
    //Deprecated
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.min.roundrobin.poll.timeout.ms",
        {{"0",true},{"10",false}});
}

TEST(ConsumerConfiguration, InternalConsumerMinPollIntervalMs)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.min.poll.interval.ms",
        {{"0",true},{"10",false}});
}

TEST(ConsumerConfiguration, InternalConsumerAutoOffsetPersist)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.auto.offset.persist",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerAutoOffsetPersistOnException)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.auto.offset.persist.on.exception",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerOffsetPersistStrategy)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.offset.persist.strategy",
        {{"bad",true},{"commit",false},{"store",false}});
}

TEST(ConsumerConfiguration, InternalConsumerCommitExec)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.commit.exec",
        {{"bad",true},{"sync",false},{"async",false}});
}

TEST(ConsumerConfiguration, InternalConsumerCommitNumRetries)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.commit.num.retries",
        {{"-1",true},{"0",false},{"1",false}});
}

TEST(ConsumerConfiguration, InternalConsumerCommitBackoffStrategy)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.commit.backoff.strategy",
        {{"bad",true},{"linear",false},{"exponential",false}});
}

TEST(ConsumerConfiguration, InternalConsumerCommitBackoffIntervalMs)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.commit.backoff.interval.ms",
        {{"0",true},{"1",false},{"2",false}});
}

TEST(ConsumerConfiguration, InternalConsumerCommitMaxBackoffMs)
{
    //Negative test
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.commit.max.backoff.ms",
        {{"100",false},{"101",false}});
    
    //Positive (< backoff.interval)
    ConsumerConfiguration config(topicWithHeaders(),
                    {{"internal.consumer.commit.backoff.interval.ms", "50"},
                     {"internal.consumer.commit.max.backoff.ms", "49"},
                     {"metadata.broker.list", programOptions()._broker},
                     {"group.id","test-group"},
                     {ConsumerConfiguration::Options::pauseOnStart, true},
                     {ConsumerConfiguration::Options::readSize, 1}}, {}, Callbacks::messageReceiverWithHeaders);
    testConnectorOption<InvalidOptionException>(config, "InvalidOptionException", "internal.consumer.commit.max.backoff.ms", true);
}

TEST(ConsumerConfiguration, InternalConsumerPollStrategy)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.poll.strategy",
        {{"bad",true},{"batch",false},{"roundRobin",false},{"serial", false}});
}

TEST(ConsumerConfiguration, InternalConsumerReadSize)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.read.size",
        {{"-2",true},{"-1",false},{"1",false}});
}

TEST(ConsumerConfiguration, InternalConsumerBatchPrefetch)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.batch.prefetch",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerReceiveCallbackThreadRangeLow)
{
    //Negative test
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.receive.callback.thread.range.low",
        {{"-1",true},{"0",false},{"1",false},{"4",false}});
    
    //Positive
    ConsumerConfiguration config(topicWithHeaders(),
                    {{"internal.consumer.receive.callback.thread.range.low", "5"},
                     {"internal.consumer.receive.callback.thread.range.high", "4"},
                     {"metadata.broker.list", programOptions()._broker},
                     {"group.id","test-group"},
                     {ConsumerConfiguration::Options::pauseOnStart, true},
                     {ConsumerConfiguration::Options::readSize, 1}}, {}, Callbacks::messageReceiverWithHeaders);
    testConnectorOption<InvalidOptionException>(config, "InvalidOptionException", "internal.consumer.receive.callback.thread.range.low", true);
}

TEST(ConsumerConfiguration, InternalConsumerReceiveCallbackThreadRangeHigh)
{
    //Negative test
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.receive.callback.thread.range.high",
        {{"-1",true},{"0",false},{"1",false},{"4",false}});
    
    //Positive
    ConsumerConfiguration config(topicWithHeaders(),
                    {{"internal.consumer.receive.callback.thread.range.low", "3"},
                     {"internal.consumer.receive.callback.thread.range.high", "2"},
                     {"metadata.broker.list", programOptions()._broker},
                     {"group.id","test-group"},
                     {ConsumerConfiguration::Options::pauseOnStart, true},
                     {ConsumerConfiguration::Options::readSize, 1}}, {}, Callbacks::messageReceiverWithHeaders);
    testConnectorOption<InvalidOptionException>(config, "InvalidOptionException", "internal.consumer.receive.callback.thread.range.high", true);
}

TEST(ConsumerConfiguration, InternalConsumerReceiveCallbackExec)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.receive.callback.exec",
        {{"bad",true},{"sync",false},{"async",false}});
}

TEST(ConsumerConfiguration, InternalConsumerReceiveInvokeThread)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.receive.invoke.thread",
        {{"bad",true},{"io",false},{"coro",false}});
}

TEST(ConsumerConfiguration, InternalConsumerLogLevel)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.log.level",
        {{"bad",true},{"emergency",false},{"CRITICAL",false},{" error ",false},{"warning",false},{"notice",false},
         {"info",false},{"debug",false}});
}

TEST(ConsumerConfiguration, InternalConsumerSkipUnknownHeaders)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.skip.unknown.headers",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerAutoThrottle)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.auto.throttle",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerAutoThrottleMultiplier)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.auto.throttle.multiplier",
        {{"0",true},{"1",false},{"2",false}});
}

TEST(ConsumerConfiguration, InternalConsumerPreprocessMessages)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.preprocess.messages",
        {{"bad",true},{"true",false},{"false",false}});
}

TEST(ConsumerConfiguration, InternalConsumerPreserveMessageOrder)
{
    testConsumerOption<InvalidOptionException>("InvalidOptionException", "internal.consumer.preserve.message.order",
        {{"bad",true},{" true ",false},{"false",false},{"FALSE",false}});
}

TEST(Consumer, ValidatePauseOnStart)
{
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
        config1,
        consumerTopicConfig,
        getNewGroupName(),
        topicWithoutHeaders(),
        Callbacks::messageReceiverWithoutHeaders,
        PartitionStrategy::Static,
        {{topicWithoutHeaders().topic(), 0, (int)OffsetPoint::AtEnd},
         {topicWithoutHeaders().topic(), 1, (int)OffsetPoint::AtEnd},
         {topicWithoutHeaders().topic(), 2, (int)OffsetPoint::AtEnd},
         {topicWithoutHeaders().topic(), 3, (int)OffsetPoint::AtEnd}});
    
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    //make sure the receiver callback nor the rebalance callbacks have been invoked
    EXPECT_EQ(0, callbackCounters()._receiver);
    EXPECT_EQ(1, callbackCounters()._assign);
    EXPECT_EQ(0, callbackCounters()._revoke);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    EXPECT_EQ(0, callbackCounters()._preprocessor);
    
    //enable consuming
    connector.consumer().resume(topicWithoutHeaders().topic());
    
    waitUntilEof();
    
    EXPECT_EQ(0, callbackCounters()._messageErrors);
    EXPECT_EQ(NumPartitions, callbackCounters()._receiver);
    EXPECT_EQ(1, callbackCounters()._assign);
    EXPECT_EQ(0, callbackCounters()._revoke);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    EXPECT_EQ(0, callbackCounters()._preprocessor);
    dispatcher().drain();
}

TEST(Consumer, ReadTopicWithoutHeadersUsingConfig1)
{
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
        config1,
        consumerTopicConfig,
        getNewGroupName(),
        topicWithoutHeaders(),
        Callbacks::messageReceiverWithoutHeaders,
        PartitionStrategy::Static,
        {{topicWithoutHeaders().topic(), 0, (int)OffsetPoint::AtBeginning},
         {topicWithoutHeaders().topic(), 1, (int)OffsetPoint::AtBeginning},
         {topicWithoutHeaders().topic(), 2, (int)OffsetPoint::AtBeginning},
         {topicWithoutHeaders().topic(), 3, (int)OffsetPoint::AtBeginning}});
    connector.consumer().resume();
    
    waitUntilEof();
    
    EXPECT_LE(10, callbackCounters()._offsetCommit);
    EXPECT_FALSE(callbackCounters()._receiverIoThread);
    EXPECT_EQ(1, callbackCounters()._assign);
    EXPECT_EQ(0, callbackCounters()._revoke);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    
    //Check message validity
    EXPECT_EQ(messageWithoutHeadersTracker(), consumerMessageWithoutHeadersTracker());
    
    //Check commits
    EXPECT_EQ(callbackCounters()._offsetCommitPartitions,
              consumerMessageWithoutHeadersTracker()._offsets);
    
    ConsumerMetadata meta = connector.consumer().getMetadata(topicWithoutHeaders().topic());
    OffsetWatermarkList water1 = meta.getOffsetWatermarks();
    OffsetWatermarkList water2 = meta.queryOffsetWatermarks(std::chrono::milliseconds(1000));
    auto off1 = meta.getOffsetPositions();
    auto off2 = meta.queryCommittedOffsets(std::chrono::milliseconds(1000));
    
    //clear everything
    consumerMessageWithoutHeadersTracker().clear();
    dispatcher().drain();
}

TEST(Consumer, ReadTopicWithHeadersUsingConfig2)
{
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
        config2,
        consumerTopicConfig,
        getNewGroupName(),
        topicWithHeaders(),
        Callbacks::messageReceiverWithHeadersManualCommit,
        PartitionStrategy::Static,
        {{topicWithHeaders().topic(), 0, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 1, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 2, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 3, (int)OffsetPoint::AtBeginning}});
    
    waitUntilEof();
    
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._preprocessor);
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._receiver-callbackCounters()._eof);
    EXPECT_FALSE(callbackCounters()._receiverIoThread);
    
    //Check message validity
    EXPECT_EQ(messageTracker(), consumerMessageTracker());
    EXPECT_EQ(messageTracker().totalMessages(), consumerMessageTracker().totalMessages());
    
    //Check async commits
    int loops = MaxLoops;
    while (static_cast<size_t>(callbackCounters()._offsetCommit) < messageTracker().totalMessages() && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._offsetCommit);
    //EXPECT_EQ(callbackCounters()._offsetCommitPartitions, consumerMessageTracker()._offsets);
    
    //clear everything
    consumerMessageTracker().clear();
    dispatcher().drain();
}

TEST(Consumer, ReadTopicWithHeadersUsingConfig3)
{
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
        config3,
        consumerTopicConfig,
        getNewGroupName(),
        topicWithHeaders(),
        Callbacks::messageReceiverWithHeadersManualCommit,
        PartitionStrategy::Static,
        {{topicWithHeaders().topic(), 0, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 1, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 2, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 3, (int)OffsetPoint::AtBeginning}});
    
    waitUntilEof();
    
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._preprocessor);
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._receiver-callbackCounters()._eof);
    EXPECT_TRUE(callbackCounters()._receiverIoThread);
    
    //Check message validity
    EXPECT_EQ(messageTracker(), consumerMessageTracker());
    EXPECT_EQ(messageTracker().totalMessages(), consumerMessageTracker().totalMessages());
    
    //Check async commits
    int loops = MaxLoops;
    while (static_cast<size_t>(callbackCounters()._offsetCommit) < messageTracker().totalMessages() && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._offsetCommit);
    EXPECT_EQ(callbackCounters()._offsetCommitPartitions, consumerMessageTracker()._offsets);
    
    //clear everything
    consumerMessageTracker().clear();
    dispatcher().drain();
}

TEST(Consumer, ReadTopicWithHeadersUsingConfig4)
{
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
        config4,
        consumerTopicConfig,
        getNewGroupName(),
        topicWithHeaders(),
        Callbacks::messageReceiverWithHeadersManualCommit,
        PartitionStrategy::Static,
        {{topicWithHeaders().topic(), 0, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 1, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 2, (int)OffsetPoint::AtBeginning},
         {topicWithHeaders().topic(), 3, (int)OffsetPoint::AtBeginning}});

    //enable consuming
    connector.consumer().resume(topicWithHeaders().topic());

    waitUntilEof();
    
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._preprocessor);
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._receiver-callbackCounters()._eof);
    EXPECT_FALSE(callbackCounters()._receiverIoThread);
    
    //Check message validity
    EXPECT_EQ(messageTracker(), consumerMessageTracker());
    EXPECT_EQ(messageTracker().totalMessages(), consumerMessageTracker().totalMessages());
    
    //Check async commits
    int loops = MaxLoops;
    while (static_cast<size_t>(callbackCounters()._offsetCommit) < messageTracker().totalMessages() && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(messageTracker().totalMessages(), callbackCounters()._offsetCommit);
    EXPECT_EQ(callbackCounters()._offsetCommitPartitions, consumerMessageTracker()._offsets);
    
    //clear everything
    consumerMessageTracker().clear();
    dispatcher().drain();
}

TEST(Consumer, SkipMessagesWithRelativeOffsetUsingConfig2)
{
    callbackCounters().reset();
    callbackCounters()._forceSkip = true;
    int msgPerPartition = 5;
    int totalMessages = msgPerPartition * NumPartitions;
    
    Connector connector = makeConsumerConnector(
        config2,
        consumerTopicConfig,
        getNewGroupName(),
        topicWithHeaders(),
        Callbacks::messageReceiverWithHeadersManualCommit,
        PartitionStrategy::Static,
        {{topicWithHeaders().topic(), 0, (int)OffsetPoint::AtEndRelative-msgPerPartition},
         {topicWithHeaders().topic(), 1, (int)OffsetPoint::AtEndRelative-msgPerPartition},
         {topicWithHeaders().topic(), 2, (int)OffsetPoint::AtEndRelative-msgPerPartition},
         {topicWithHeaders().topic(), 3, (int)OffsetPoint::AtEndRelative-msgPerPartition}});
    
    waitUntilEof();
    
    EXPECT_EQ(totalMessages, callbackCounters()._preprocessor);
    EXPECT_EQ(totalMessages, callbackCounters()._receiver-callbackCounters()._eof); //excluding EOFs
    EXPECT_EQ(totalMessages, callbackCounters()._skip);
    EXPECT_EQ(0, consumerMessageTracker().totalMessages());
    
    //clear everything
    consumerMessageTracker().clear();
    dispatcher().drain();
}

TEST(Consumer, OffsetCommitManagerFromBeginning)
{
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
            config4,
            consumerTopicConfig,
            getNewGroupName(),
            topicWithHeaders(),
            Callbacks::messageReceiverWithHeadersUsingCommitGuard,
            PartitionStrategy::Static,
            {{topicWithHeaders().topic(), 0, (int)OffsetPoint::AtBeginning},
             {topicWithHeaders().topic(), 1, (int)OffsetPoint::AtBeginning},
             {topicWithHeaders().topic(), 2, (int)OffsetPoint::AtBeginning},
             {topicWithHeaders().topic(), 3, (int)OffsetPoint::AtBeginning}});

    //Create the offset manager
    offsetManagerPtr = std::make_shared<OffsetManager>(connector.consumer());

    //enable consuming
    connector.consumer().resume(topicWithHeaders().topic());

    waitUntilEof();
    
    //Check commits via offset manager
    int loops = MaxLoops;
    while (static_cast<size_t>(callbackCounters()._offsetCommit) < consumerMessageTracker().totalMessages() && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(callbackCounters()._offsetCommit, consumerMessageTracker().totalMessages());
    EXPECT_EQ(callbackCounters()._offsetCommitPartitions, consumerMessageTracker()._offsets);

    //clear everything
    consumerMessageTracker().clear();
    dispatcher().drain();
    offsetManagerPtr.reset();
}

TEST(Consumer, OffsetCommitManagerRelative)
{
    int relativeOffset = 3;
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
            config4,
            consumerTopicConfig,
            getNewGroupName(),
            topicWithHeaders(),
            Callbacks::messageReceiverWithHeadersUsingCommitGuard,
            PartitionStrategy::Static,
            {{topicWithHeaders().topic(), 0, (int)OffsetPoint::AtEndRelative-relativeOffset},
             {topicWithHeaders().topic(), 1, (int)OffsetPoint::AtEndRelative-relativeOffset},
             {topicWithHeaders().topic(), 2, (int)OffsetPoint::AtEndRelative-relativeOffset},
             {topicWithHeaders().topic(), 3, (int)OffsetPoint::AtEndRelative-relativeOffset}});

    //Create the offset manager
    offsetManagerPtr = std::make_shared<OffsetManager>(connector.consumer());

    //enable consuming
    connector.consumer().resume(topicWithHeaders().topic());

    waitUntilEof();
    
    //Check commits via offset manager
    int loops = MaxLoops;
    while ((callbackCounters()._offsetCommit < (relativeOffset * NumPartitions)) &&
           loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(relativeOffset * NumPartitions, callbackCounters()._offsetCommit);
    EXPECT_EQ(callbackCounters()._offsetCommitPartitions, consumerMessageTracker()._offsets);

    //clear everything
    consumerMessageTracker().clear();
    dispatcher().drain();
    offsetManagerPtr.reset();
}

TEST(Consumer, OffsetCommitManagerFromStored)
{
    int relativeOffset = 6; //total messages consumed per partition
    int firstBatch = 2; //messages consumed per partition for each test
    
    const std::string& consumerGroup = getNewGroupName();
    {
        callbackCounters().reset();
        
        //Stop committing after specific amount of offsets are received.
        callbackCounters()._maxProcessedOffsets = firstBatch;
        
        Connector connector = makeConsumerConnector(
                config4,
                consumerTopicConfig,
                consumerGroup,
                topicWithHeaders(),
                Callbacks::messageReceiverWithHeadersUsingCommitGuard,
                PartitionStrategy::Static,
                {{topicWithHeaders().topic(), 0, (int) OffsetPoint::AtEndRelative - relativeOffset},
                 {topicWithHeaders().topic(), 1, (int) OffsetPoint::AtEndRelative - relativeOffset},
                 {topicWithHeaders().topic(), 2, (int) OffsetPoint::AtEndRelative - relativeOffset},
                 {topicWithHeaders().topic(), 3, (int) OffsetPoint::AtEndRelative - relativeOffset}});
        
        //Create the offset manager
        offsetManagerPtr = std::make_shared<OffsetManager>(connector.consumer());
        
        //enable consuming
        connector.consumer().resume(topicWithHeaders().topic());
        
        waitUntilEof();
    
        //Check commits via offset manager
        int loops = MaxLoops;
        while ((callbackCounters()._offsetCommit < (callbackCounters()._maxProcessedOffsets * NumPartitions)) &&
               loops--) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        EXPECT_EQ(callbackCounters()._offsetCommit, callbackCounters()._maxProcessedOffsets*NumPartitions);
        //EXPECT_EQ(callbackCounters()._offsetCommitPartitions, callbackCounters()._numOffsetCommitted);
        
        //clear everything
        consumerMessageTracker().clear();
        dispatcher().drain();
        offsetManagerPtr.reset();
    }
    
    //Continue reading the remaining offsets using the same consumer group
    int remaining = relativeOffset-firstBatch; //messages left to read per partition
    {
        callbackCounters().reset();
        
        Connector connector = makeConsumerConnector(
                config4,
                consumerTopicConfig,
                consumerGroup,
                topicWithHeaders(),
                Callbacks::messageReceiverWithHeadersUsingCommitGuard,
                PartitionStrategy::Static,
                {{topicWithHeaders().topic(), 0, (int) OffsetPoint::FromStoredOffset},
                 {topicWithHeaders().topic(), 1, (int) OffsetPoint::FromStoredOffset},
                 {topicWithHeaders().topic(), 2, (int) OffsetPoint::FromStoredOffset},
                 {topicWithHeaders().topic(), 3, (int) OffsetPoint::FromStoredOffset}});
        
        //Create the offset manager
        offsetManagerPtr = std::make_shared<OffsetManager>(connector.consumer());
        
        //enable consuming
        connector.consumer().resume(topicWithHeaders().topic());
        
        waitUntilEof();
    
        //Check commits via offset manager
        int loops = MaxLoops;
        while ((callbackCounters()._offsetCommit < (remaining * NumPartitions)) &&
               loops--) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        EXPECT_EQ(remaining * NumPartitions, callbackCounters()._offsetCommit);
        EXPECT_EQ(callbackCounters()._offsetCommitPartitions, consumerMessageTracker()._offsets);
        //EXPECT_EQ(callbackCounters()._offsetCommitPartitions, callbackCounters()._numOffsetCommitted);
        
        //clear everything
        consumerMessageTracker().clear();
        dispatcher().drain();
        offsetManagerPtr.reset();
    }
}

TEST(Consumer, ValidateDynamicAssignment)
{
    std::string groupName = getNewGroupName();
    callbackCounters().reset();
    Connector connector = makeConsumerConnector(
        config1,
        consumerTopicConfig,
        groupName,
        topicWithoutHeaders(),
        Callbacks::messageReceiverWithoutHeaders,
        PartitionStrategy::Dynamic);
    int loops = MaxLoops;
    while (callbackCounters()._assign == 0 && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(0, callbackCounters()._messageErrors);
    //EXPECT_EQ(NumPartitions, callbackCounters()._receiver);
    EXPECT_EQ(1, callbackCounters()._assign);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    callbackCounters().reset();
    
    //Create a 2nd connector for the same group
    Connector connector2 = makeConsumerConnector(
        config1,
        consumerTopicConfig,
        groupName,
        topicWithoutHeaders(),
        Callbacks::messageReceiverWithoutHeaders,
        PartitionStrategy::Dynamic);
    loops = MaxLoops;
    while (callbackCounters()._assign < 2 && loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(0, callbackCounters()._messageErrors);
    //EXPECT_EQ(NumPartitions, callbackCounters()._receiver);
    EXPECT_EQ(2, callbackCounters()._assign);
    EXPECT_EQ(1, callbackCounters()._revoke);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    
    //resume consumption from both consumers
    connector.consumer().resume(topicWithoutHeaders().topic());
    connector2.consumer().resume(topicWithoutHeaders().topic());
    
    //Wait to consume all messages
    waitUntilEof();
    callbackCounters().reset();
    
    //stop the first connector
    connector.consumer().shutdown();
    loops = MaxLoops;
    //we expect 2 revocations and one new assignment
    while (callbackCounters()._revoke < 2 &&
           callbackCounters()._assign < 1 &&
           loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(1, callbackCounters()._assign);
    EXPECT_EQ(2, callbackCounters()._revoke);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    
     callbackCounters().reset();
    
    //stop the second consumer
    //we expect one revocations
    loops = MaxLoops;
    connector2.consumer().shutdown();
    while (callbackCounters()._revoke == 0 &&
           loops--) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(0, callbackCounters()._assign);
    EXPECT_EQ(1, callbackCounters()._revoke);
    EXPECT_EQ(0, callbackCounters()._rebalanceErrors);
    
    //stop the second connector
    dispatcher().drain();
}


}
}
}

