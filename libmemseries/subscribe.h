#pragma once

#include "meas.h"
#include "storage.h"
#include <memory>
#include <mutex>

namespace memseries {
	namespace storage {

		struct SubscribeInfo {
			IdArray ids;
			Flag flag;
			mutable ReaderClb_ptr clbk;
			bool isYours(const memseries::Meas&m) const;
		};

		typedef std::shared_ptr<SubscribeInfo> SubscribeInfo_ptr;

		struct SubscribeNotificator {
			std::list<SubscribeInfo_ptr> _subscribes;
			bool is_stoped;
			std::mutex _mutex;

			SubscribeNotificator() = default;
			~SubscribeNotificator();
			//TODO must work in thread;
			void start();
			void stop();
			void add(const SubscribeInfo_ptr&n);
			void on_append(const memseries::Meas&m)const;
		};
	}
}